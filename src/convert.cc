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

#include "sst/core/sst_config.h"

#include "convert.h"


using namespace SST;
using namespace SST::Swm;

std::map<int,int> g_verboseLevel;
std::map<int,int> g_verboseMask;

const char* Convert::m_functionName[] = {
    FOREACH_FUNCTION(GENERATE_STRING)
};

Convert::Convert( Link* link, MP::Interface* mp, int jobId, int rank, uint32_t verboseLevel, uint32_t verboseMask ): 
	m_selfLink(link), m_mp(mp), m_jobId(jobId), m_rank(rank), m_type(Empty), m_reqNum(0),
	initFunctor(Functor(this, &Convert::handleReturn, Init)),
	finiFunctor(Functor(this, &Convert::handleReturn, Finalize)),
	sendFunctor(Functor(this, &Convert::handleReturn, Send)),
	isendFunctor(Functor(this, &Convert::handleReturn, Isend)),
	recvFunctor(Functor(this, &Convert::handleReturn, Recv)),
	irecvFunctor(Functor(this, &Convert::handleReturn, Irecv)),
	sendrecvFunctor(Functor(this, &Convert::handleReturn, SendRecv)),
	sendrecvIrecvFunctor(Functor(this, &Convert::handleSendRecvIrecvReturn,0)),
	sendrecvSendFunctor(Functor(this, &Convert::handleSendRecvSendReturn,0)),
	waitallFunctor(Functor(this, &Convert::handleReturn, Waitall)),
	waitFunctor(Functor(this, &Convert::handleReturn, Wait)),
	allreduceFunctor(Functor(this, &Convert::handleReturn, Allreduce)),
	barrierFunctor(Functor(this, &Convert::handleReturn, Barrier))
{
	g_verboseLevel[jobId] = verboseLevel;
	g_verboseMask[jobId] = verboseMask;
    char buffer[100];
    snprintf(buffer,100,"@t:%d:%d:Convert::@p():@l ",jobId,m_rank);
    m_output.init(buffer, verboseLevel, verboseMask, Output::STDOUT);
}

bool Convert::handleSendRecvIrecvReturn( int retval, int type) {
    m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"send peer=%d comm_id=%d tag=%#x bytes=%d\n",
                 (int)m_args.sendrecv.sendpeer,(int)m_args.sendrecv.comm_id,(int)m_args.sendrecv.sendtag,(int)m_args.sendrecv.sendbytes);
    Hermes::MemAddr addr(0,NULL);
	m_mp->send( addr, m_args.sendrecv.sendbytes, CHAR, m_args.sendrecv.sendpeer, m_args.sendrecv.sendtag, m_args.sendrecv.comm_id, &sendrecvSendFunctor );
    return false;
}

bool Convert::handleSendRecvSendReturn( int retval, int type) {
    m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"wait\n");
    m_resp.resize( 1 );
	m_mp->wait( m_req[0], &m_resp[0], &sendrecvFunctor);
    return false;
}

bool Convert::handleReturn( int retval, int type) {
    m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"%s returned\n",m_functionName[type],retval);
    m_selfLink->send( new SwmEvent(SwmEvent::Type::MP_Returned, retval, type ) );
    return false;
}

void Convert::MP_returned( int retval, int  type) {
	m_output.debug(CALL_INFO, 3, SWM_CONVERT_DBG_MASK,"thread=%" PRIx64 " %s retval=%d\n",std::this_thread::get_id(),m_functionName[type],retval);
    signalWorkload();
    waitForWork();
    doWork();
}

void Convert::doWork() {

    m_output.debug(CALL_INFO, 2, SWM_CONVERT_DBG_MASK,"thread=%" PRIx64 " got %s\n",std::this_thread::get_id(),m_functionName[m_type]);
    switch ( m_type ) {
      case Exit:
		m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"exit\n");
        m_selfLink->send( new SwmEvent(SwmEvent::Type::Exit ) );
        break;
      case Init: 
		m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"init\n");
        m_mp->init( &initFunctor );
        break;
      case Finalize: 
		m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"finalize\n");
        m_mp->fini( &finiFunctor );
        break;
      case Send:
        {
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"send peer=%d comm_id=%d tag=%#x bytes=%d\n",
                    (int)m_args.send.peer,(int)m_args.send.comm_id,(int)m_args.send.tag,(int)m_args.send.bytes);
            Hermes::MemAddr addr(0,NULL);
            m_mp->send( addr, m_args.send.bytes, CHAR, m_args.send.peer, m_args.send.tag, m_args.send.comm_id, &sendFunctor );
        }
        break;
      case Isend:
        {
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"isend peer=%d comm_id=%d tag=%#x bytes=%d handle=%d\n",
                    (int)m_args.send.peer,(int)m_args.send.comm_id,(int)m_args.send.tag,(int)m_args.send.bytes,*m_args.send.handle);
            Hermes::MemAddr addr(0,NULL);
            uint32_t num;
            MessageRequest* req;
            std::tie(num,req) = allocMsgReq();
            *m_args.send.handle = num;
            m_output.debug(CALL_INFO, 2, SWM_CONVERT_DBG_MASK,"Isend handle=%d req=%p\n",num,req);
            m_mp->isend( addr, m_args.send.bytes, CHAR, m_args.send.peer, m_args.send.tag, m_args.send.comm_id, req, &isendFunctor );
        }
        break;
      case Recv:
        {
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"recv peer=%d comm_id=%d tag=%#x bytes=%d \n",m_args.recv.peer,m_args.recv.comm_id,m_args.recv.tag,m_args.recv.bytes);
	        Hermes::MemAddr addr(0,NULL);
            m_resp.resize(1);
	        m_mp->recv( addr, m_args.recv.bytes, CHAR, m_args.recv.peer, m_args.recv.tag, m_args.recv.comm_id, m_resp.data(), &recvFunctor );
        }
        break;
      case Irecv:
        {
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"irecv peer=%d comm_id=%d tag=%#x bytes=%d \n",m_args.recv.peer,m_args.recv.comm_id,m_args.recv.tag,m_args.recv.bytes);
	        Hermes::MemAddr addr(0,NULL);
            uint32_t num;
            MessageRequest* req;
            std::tie(num,req) = allocMsgReq();
            *m_args.recv.handle = num;
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"Irecv handle=%d %p\n",num,req);
	        m_mp->irecv( addr, m_args.recv.bytes, CHAR, m_args.recv.peer, m_args.recv.tag, m_args.recv.comm_id, req, &irecvFunctor );
        }
        break;
      case SendRecv:
		{
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"sendrecv comm_id=%d sendpeer=%d sendtag=%#x sendbytes=%d recvpeer=%d recvtag=%#x\n",
				m_args.sendrecv.comm_id, m_args.sendrecv.sendpeer, m_args.sendrecv.sendtag, m_args.sendrecv.sendbytes, m_args.sendrecv.recvpeer, m_args.sendrecv.recvtag );
	        Hermes::MemAddr addr(0,NULL);
			m_req.resize(1);
	        m_mp->irecv( addr, m_args.sendrecv.sendbytes, CHAR, m_args.sendrecv.recvpeer, m_args.sendrecv.recvtag, m_args.sendrecv.comm_id, &m_req[0], &sendrecvIrecvFunctor );
		}
		break;
      case Wait: 
		{
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"wait\n");
            m_req.resize( 1 );
            m_resp.resize( 1 );
            m_req[0] = *findMsgReq( m_args.wait.req_id ); 
            freeMsgReq( m_args.wait.req_id ); 
	        m_mp->wait( m_req[0], &m_resp[0], &waitFunctor);
		}
		break;
      case Waitall: 
        {
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"waitall len=%d\n",m_args.waitall.len);
            m_req.resize( m_args.waitall.len );
            m_resp.resize( m_args.waitall.len );
            for ( int i = 0; i < m_args.waitall.len; i++ ) {
                m_req[i] = *findMsgReq( m_args.waitall.req_ids[i] ); 
                m_output.debug(CALL_INFO, 2, SWM_CONVERT_DBG_MASK,"id=%d req=%p\n",m_args.waitall.req_ids[i],m_req[i]);
                freeMsgReq( m_args.waitall.req_ids[i] ); 
            }
	        m_mp->waitall( m_args.waitall.len, m_req.data(), (MessageResponse**) m_resp.data(), &waitallFunctor);
        }
		break;
      case Allreduce: 
		{
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"allreduce bytes=%d\n",m_args.allreduce.bytes);
	        Hermes::MemAddr addr(0,NULL);
			m_mp->allreduce( addr, addr, m_args.allreduce.bytes, CHAR, NOP, m_args.allreduce.comm_id, &allreduceFunctor );
		}
        break;
      case Barrier: 
		{
            m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"barrier\n");
	        Hermes::MemAddr addr(0,NULL);
			m_mp->barrier( m_args.barrier.comm_id, &barrierFunctor );
		}
        break;
      case Compute: 
        m_output.debug(CALL_INFO, 1, SWM_CONVERT_DBG_MASK,"compute ns=%f\n",m_args.compute.ns);
        m_selfLink->send( (SimTime_t)m_args.compute.ns, new SwmEvent(SwmEvent::Type::MP_Returned, 0, m_type ) );
        break;
    }
}
