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

#include <map>
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <assert.h>
#include <mutex>

#include "sst/core/sst_config.h"

#include <boost/property_tree/json_parser.hpp>
#include <swm-include.h>
#include "workload.h"

using namespace SST;
using namespace SST::Swm;

std::map<int,boost::property_tree::ptree> Workload::m_root;

std::map<int,std::mutex> Workload::m_mutex;
std::map<int,bool> Workload::m_readConfig;

Workload::Workload( Convert* convert, std::string path, std::string name, int numRanks, int jobId, int rank, uint32_t verboseLevel, uint32_t verboseMask) :
	m_convert(convert), m_numRanks(numRanks), m_jobId(jobId), m_rank(rank), m_dbgLvl(verboseLevel), m_dbgMask(verboseMask)
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:Workload::@p():@l ",m_rank);
    m_output.init(buffer, verboseLevel, verboseMask, Output::STDOUT);

    void** generic_ptrs;
    int array_len = 1;
    generic_ptrs = (void**)calloc(array_len,  sizeof(void*));
    generic_ptrs[0] = (void*)&rank;

	if ( m_readConfig.find(jobId) == m_readConfig.end() ) {
		m_readConfig[jobId] = true;
		m_root[jobId];
		m_mutex[jobId];
	}

	m_output.debug( CALL_INFO, 1, SWM_WORKLOAD_DBG_BITS, "path=%s workload=%s\n",path.c_str(),name.c_str());

	boost::property_tree::ptree& root = m_root[jobId];

	std::unique_lock<std::mutex> lck(m_mutex[jobId]);
	if ( m_readConfig[jobId] ) {
    	try {
            std::ifstream jsonFile(path.c_str());
            boost::property_tree::json_parser::read_json(jsonFile, root);
            jsonFile.close();

            root.put("jobs.size", m_numRanks);
    	}
    	catch(std::exception & e)
    	{
			throw;
    	}
        m_readConfig[jobId] = false;
	}
    lck.unlock();

    m_cpuFreq = root.get<double>("jobs.cfg.cpu_freq") / 1e9;

    if( name.compare( "lammps") == 0)
    {
        m_type = Lammps;
        m_lammps = new LAMMPS_SWM(root, generic_ptrs);
    }
    else if ( name.compare( "nekbone") == 0)
    {
        m_type = Nekbone;
        m_nekbone = new NEKBONESWMUserCode(root, generic_ptrs);
    }
    else if ( name.compare( "nearest_neighbor") == 0)
    {
        m_type = NN;
        m_nn = new NearestNeighborSWMUserCode(root, generic_ptrs);
    }
    else if ( name.compare( "many_to_many") == 0)
    {
        m_type = MM;
        m_mm = new ManyToManySWMUserCode(root, generic_ptrs);
    }
    else if ( name.compare( "milc") == 0)
    {
        m_type = MILC;
        m_milc = new MilcSWMUserCode(root, generic_ptrs);
    }
    else if ( name.compare( "incast") == 0 || name.compare( "incast1") == 0 || name.compare( "incast2") == 0)
    {
        m_type = Incast;
        m_incast = new AllToOneSWMUserCode(root, generic_ptrs);
    } else {
		throw "Unknown workload: " + name;
	}	
}

static thread_local Workload* tl_workload;

static void workloadThread( Workload* workload ) {
	tl_workload = workload;

	WorkloadDBG( tl_workload, "call init()\n");
	tl_workload->convert().init( );

	tl_workload->call();

	WorkloadDBG( tl_workload, "workload returned, call exit()\n");
	tl_workload->convert().exit();
}

void Workload::start() { 
	m_output.debug( CALL_INFO, 1, SWM_WORKLOAD_DBG_BITS, "start thread\n");
	m_thread = std::thread( workloadThread, this ); 
    m_convert->waitForWork();
    m_convert->doWork();
}

void Workload::stop() { 
	m_output.debug( CALL_INFO, 1, SWM_WORKLOAD_DBG_BITS, "stop thread\n");
	m_thread.join(); 
}

void SWM_Init() 
{
    // Some workload don't call this but we need it so we call it when the thread is started
}

void SWM_Send(SWM_PEER peer,
              SWM_COMM_ID comm_id,
              SWM_TAG tag,
              SWM_VC reqvc,
              SWM_VC rspvc,
              SWM_BUF buf,
              SWM_BYTES bytes,
              SWM_BYTES pktrspbytes,
              SWM_ROUTING_TYPE reqrt,
              SWM_ROUTING_TYPE rsprt)
{
	WorkloadDBG(tl_workload, "peer=%d comm_id=%d tag=%#x bytes=%d\n",peer,comm_id,tag,bytes);
	tl_workload->convert().send( peer, comm_id, tag, reqvc, rspvc, buf, bytes, pktrspbytes, reqrt, rsprt );
}

void SWM_Isend(SWM_PEER peer,
              SWM_COMM_ID comm_id,
              SWM_TAG tag,
              SWM_VC reqvc,
              SWM_VC rspvc,
              SWM_BUF buf,
              SWM_BYTES bytes,
              SWM_BYTES pktrspbytes,
              uint32_t * handle,
              SWM_ROUTING_TYPE reqrt,
              SWM_ROUTING_TYPE rsprt)
{
	WorkloadDBG(tl_workload, "peer=%d comm_id=%d tag=%#x bytes=%d handle=%d\n",peer,comm_id,tag,bytes,*handle);
	tl_workload->convert().isend( peer, comm_id, tag, reqvc, rspvc, buf, bytes, pktrspbytes, handle, reqrt, rsprt );
}

void SWM_Barrier(
        SWM_COMM_ID comm_id,
        SWM_VC reqvc,
        SWM_VC rspvc,
        SWM_BUF buf,
        SWM_UNKNOWN auto1,
        SWM_UNKNOWN2 auto2,
        SWM_ROUTING_TYPE reqrt,
        SWM_ROUTING_TYPE rsprt)
{
	WorkloadDBG(tl_workload, "comm_id=%d reqvc=%d rspvc=%d auto1=%d auto2=%d reqrt=%d rsprt=%d\n",
            comm_id,reqvc,rspvc,auto1,auto2,reqrt,rsprt);
	tl_workload->convert().barrier( comm_id, reqvc, rspvc, buf, auto1, auto2, reqrt, rsprt );
}

void SWM_Recv(SWM_PEER peer,
        SWM_COMM_ID comm_id,
        SWM_TAG tag,
        SWM_BUF buf,
        SWM_BYTES bytes)
{
	WorkloadDBG(tl_workload, "peer=%d comm_id=%d tag=%#x bytes=%d\n",peer,comm_id,tag,bytes);
	tl_workload->convert().recv( peer, comm_id, tag, buf, bytes );
}

void SWM_Irecv(SWM_PEER peer,
        SWM_COMM_ID comm_id,
        SWM_TAG tag,
        SWM_BUF buf,
        SWM_BYTES bytes,
        uint32_t* handle)
{
	WorkloadDBG(tl_workload, "peer=%d comm_id=%d tag=%#x bytes=%d handle=%d\n",peer,comm_id,tag,bytes,*handle);
	tl_workload->convert().irecv( peer, comm_id, tag, buf, bytes, handle );
}


void SWM_Compute(long cycle_count)
{
	WorkloadDBG(tl_workload, "cycle_count=%lu\n",cycle_count);
	tl_workload->convert().compute( tl_workload->calcComputeTime(cycle_count ) );
}

void SWM_Wait(uint32_t req_id)
{
	WorkloadDBG(tl_workload, "\n");
	tl_workload->convert().wait( req_id );
}

void SWM_Waitall(int len, uint32_t * req_ids)
{
	WorkloadDBG(tl_workload, "len=%d\n",len);
	tl_workload->convert().waitall( len, req_ids );
}

void SWM_Sendrecv(
         SWM_COMM_ID comm_id,
         SWM_PEER sendpeer,
         SWM_TAG sendtag,
         SWM_VC sendreqvc,
         SWM_VC sendrspvc,
         SWM_BUF sendbuf,
         SWM_BYTES sendbytes,
         SWM_BYTES pktrspbytes,
         SWM_PEER recvpeer,
         SWM_TAG recvtag,
         SWM_BUF recvbuf,
         SWM_ROUTING_TYPE reqrt,
         SWM_ROUTING_TYPE rsprt )
{
	WorkloadDBG(tl_workload, "comm_id=%d sendpeer=%d sendtag=%#x sendbytes=%d recvpeer=%d recvtag=%d \n",
			comm_id,sendpeer,sendtag,sendbytes,recvpeer,recvtag);
	tl_workload->convert().sendrecv( comm_id, sendpeer, sendtag, sendreqvc, sendrspvc, sendbuf, sendbytes, pktrspbytes, recvpeer, recvtag, recvbuf, reqrt, rsprt);
}

void SWM_Allreduce(
        SWM_BYTES bytes,
        SWM_BYTES rspbytes,
        SWM_COMM_ID comm_id,
        SWM_VC sendreqvc,
        SWM_VC sendrspvc,
        SWM_BUF sendbuf,
        SWM_BUF rcvbuf)
{
	WorkloadDBG(tl_workload, "bytes=%d rspbytes=%d comm_id=%d sendreqvc=%d sendrspvc=%d\n",
            bytes,rspbytes,comm_id,sendreqvc,sendrspvc,sendbuf,rcvbuf);
	tl_workload->convert().allreduce( bytes, rspbytes, comm_id, sendreqvc, sendrspvc, sendbuf, rcvbuf, 0, 0, 0, 0 );
}

void SWM_Allreduce(
        SWM_BYTES bytes,
        SWM_BYTES rspbytes,
        SWM_COMM_ID comm_id,
        SWM_VC sendreqvc,
        SWM_VC sendrspvc,
        SWM_BUF sendbuf,
        SWM_BUF rcvbuf,
        SWM_UNKNOWN auto1,
        SWM_UNKNOWN2 auto2,
        SWM_ROUTING_TYPE reqrt,
        SWM_ROUTING_TYPE rsprt)
{
	WorkloadDBG(tl_workload, "bytes=%d rspbytes=%d comm_id=%d sendreqvc=%d sendrspvc=%d auto1=%d auto2=%d reqrt=%d rsprt=%d\n",
            bytes,rspbytes,comm_id,sendreqvc,sendrspvc,sendbuf,rcvbuf,auto1,auto2,reqrt,rsprt);
	tl_workload->convert().allreduce( bytes, rspbytes, comm_id, sendreqvc, sendrspvc, sendbuf, rcvbuf, auto1, auto2, reqrt, rsprt );
}

void SWM_Finalize()
{
	WorkloadDBG(tl_workload, "\n");
	tl_workload->convert().finalize();
}

