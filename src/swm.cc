// Copyright 2009-2021 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2021, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <string>
#include "sst/core/sst_config.h"

#include <sstream>

#include "swm.h"
#include "convert.h"

/*
    Install the python library
*/
#include <sst/core/model/element_python.h>

namespace SST {
namespace Swm {

char pyswm[] = {
#include "pyswm.inc"
    0x00};

class SwmPyModule : public SSTElementPythonModule {
public:
    SwmPyModule(std::string library) :
        SSTElementPythonModule(library)
    {
        auto primary_module = createPrimaryModule(pyswm,"pyswm.py");
    }

    SST_ELI_REGISTER_PYTHON_MODULE(
        SST::Swm::SwmPyModule,
        "sstSwm",
        SST_ELI_ELEMENT_VERSION(1,0,0)
    )

    SST_ELI_EXPORT(SST::Swm::SwmPyModule)
};

}
}

using namespace SST;
using namespace SST::Swm;

SwmComponent::SwmComponent(ComponentId_t id, Params& params ) : Component( id )
{
    m_verboseLevel = params.find<uint32_t>("verboseLevel",0);
    m_verboseMask = params.find<uint32_t>("verboseMask",-1);
    m_jobId = params.find<int>("jobId",-1);
    sst_assert(-1 != m_jobId, CALL_INFO,-1,"parameter jobId was not set\n"); 

    m_path = params.find<std::string>("path");
    m_workloadName = params.find<std::string>("name");

    char buffer[100];
    snprintf(buffer,100,"SwmComponent::@p():@l ");
    Output output(buffer, m_verboseLevel, m_verboseMask, Output::STDOUT);

    m_numRanks = params.find<int>("numRanks",0);
    if ( ! m_numRanks ) {
        output.fatal(CALL_INFO,-1,"numRanks was not set\n"); 
    }

    m_os = loadUserSubComponent<OS>( "OS" );
    if( ! m_os ) {
		output.fatal(CALL_INFO,-1,"Couldn't load the \"OS\" SubComponent\n"); 
    }

    Params osParams = params.find_prefix_params("os.");
    std::string osName = osParams.find<std::string>("name");
    Params modParams = params.find_prefix_params( osName + "." );
    m_msgapi = loadAnonymousSubComponent<MP::Interface>( "firefly.hadesMP", "", 0, ComponentInfo::SHARE_NONE, modParams );
    if( ! m_msgapi ) {
		output.fatal(CALL_INFO,-1,"Couldn't load the \"firefly.hadesMP\" SubComponent\n"); 
    }

    m_msgapi->setOS( m_os );

    m_selfLink = configureSelfLink("Self", "1ns", new Event::Handler<SwmComponent>(this, &SwmComponent::handleSelfEvent));

    m_tConv = Simulation::getSimulation()->getTimeLord()->getTimeConverter("1ns");

    // Make sure we don't stop the simulation until we are ready
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();
}

SwmComponent::~SwmComponent() {
	delete m_workload;
    delete m_convert;
}

void SwmComponent::setup() {
    m_output.debug(CALL_INFO, 1, SWM_DBG_MASK,"enter\n");
    m_os->_componentSetup();
    m_rank = m_os->getRank();
    m_output.debug(CALL_INFO, 1, SWM_DBG_MASK,"rank %d\n",m_rank);

    char buffer[100];
    snprintf(buffer,100,"@t:%d:%d:SwmComponent::@p():@l ",m_jobId,m_rank);
    m_output.init(buffer, m_verboseLevel, m_verboseMask, Output::STDOUT);

	m_convert = new Convert( m_selfLink, m_msgapi, m_jobId, m_rank, m_verboseLevel, m_verboseMask );

    try {
		m_workload = new Workload( m_convert, m_path, m_workloadName, m_numRanks, m_jobId, m_rank, m_verboseLevel, m_verboseMask );
    }
    catch(std::exception & e)
    {
		m_output.fatal(CALL_INFO,-1,"could not create workload=%s file=%s, \"%s\"\n",m_workloadName.c_str(),m_path.c_str(), e.what()); 
    }

    m_msgapi->setup();

    m_selfLink->send( new SwmEvent(SwmEvent::Type::StartWorkload) );

    m_output.debug(CALL_INFO, 1, SWM_DBG_MASK,"return\n");
}

void SwmComponent::handleSelfEvent( Event* ev ) {
    SwmEvent* event = static_cast< SwmEvent* >(ev);
    m_output.debug(CALL_INFO, 2, SWM_DBG_MASK,"type=%d\n",event->type);
    switch ( event->type ) {
      case SwmEvent::Type::StartWorkload:
        m_workload->start();
        break;
      case SwmEvent::Type::MP_Returned:
        m_convert->MP_returned(event->arg1,event->arg2);
        break;
      case SwmEvent::Type::Exit:
        m_output.debug(CALL_INFO, 1, SWM_DBG_MASK,"call primaryComponentOKToEndSim()\n",event->type);
        primaryComponentOKToEndSim();
        break;
    }
}
