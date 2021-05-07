#!/usr/bin/env python
#
# Copyright 2009-2021 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2021, NTESS
# All rights reserved.
#
# Portions are copyright of other developers:
# See the file CONTRIBUTORS.TXT in the top level directory
# the distribution for more information.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst
from sst.merlin.base import *
from sst.firefly import *

class SwmJob(Job):
    def __init__(self,job_id,num_nodes):
        Job.__init__(self,job_id,num_nodes)
        self._declareParams("main",["_os","_numCores","_nicsPerNode","nic"])

        self._declareParamsWithUserPrefix("workload","workload",["verboseLevel","verboseMask","numRanks","path","name"])

        self._nicsPerNode = 1
        self._numCores = 1

        # Instance the OS layer and lock it (make it read only)
        self._os = FireflyHades()
        self._lockVariable("_os")

        self.nic = BasicNicConfiguration()
        self._lockVariable("nic")

    def getName(self):
        return "SwmJob"

    def build(self, nodeID, extraKeys):

        if self._check_first_build():
            sst.addGlobalParams("lookback_params_%s"%self._instance_name,
                            { "numCores" : self._numCores,
                              "nicsPerNode" : self._nicsPerNode })

            sst.addGlobalParam("params_%s"%self._instance_name, 'jobId', self.job_id)
            sst.addGlobalParams("params_%s"%self._instance_name, self._getGroupParams("workload"))

        logical_id = self._nid_map[nodeID]
        core = 0
        nodeNicNum = 0

        nic, slot_name = self.nic.build(nodeID)	

        networkif, port_name = self.network_interface.build(nic,slot_name,0,self.job_id,self.size,logical_id,False)

        # Store return value for later
        retval = ( networkif, port_name )

        loopBack = sst.Component("loopBack" + str(nodeID), "firefly.loopBack")
  
        # per core starts here, we currntly only support 1 core
        ep = sst.Component("nic" + str(nodeID) + "core" + str(core) + "_SWM", "sstSwm.Swm")
        self._applyStatisticsSettings(ep)
        ep.addGlobalParamSet("params_%s"%self._instance_name )

        # Create the links to the OS layer
        nicLink = sst.Link( "nic" + str(nodeID) + "core" + str(core) + "_Link"  )
        nicLink.setNoCut()
        nic.addLink(nicLink,'core'+ str(core),'1ns')

        loopLink = sst.Link( "loop" + str(nodeID) + "nic" + str(nodeNicNum) + "core" + str(core) + "_Link"  );
        loopLink.setNoCut()
        loopBack.addLink(loopLink,'nic'+str(nodeNicNum)+'core'+str(core),'1ns')

        # Create the OS layer
        self._os.build(ep,nicLink,loopLink,self.size,self._nicsPerNode,self.job_id,nodeID,logical_id,core)

        return retval 
