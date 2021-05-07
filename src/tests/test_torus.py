#!/usr/bin/env python
#
# Copyright 2009-2021 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2021, NTESS
# All rights reserved.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst
from sst.merlin.base import *
from sst.merlin.endpoint import *
from sst.merlin.interface import *
from sst.merlin.topology import *

from sst.sstSwm import *

if __name__ == "__main__":

    PlatformDefinition.setCurrentPlatform("firefly-defaults")

    w=4
    x=4
    y=4
    z=2
    numNodes=w*x*y*z
    numJobs=2
    print( "numNodes ", numNodes );

    numNodesJob1 = 20
    numNodesJob2 = numNodes - numNodesJob1
    numRanksJob1 = numNodesJob1
    ### Setup the topology
    topo = topoTorus()

    topo.link_latency = "40ns"
    topo.shape = str(w) + "x" + str(x) + "x" + str(y) + "x" + str(z)
    topo.width = "1x1x1x1"
    topo.local_ports = 1

    # Set up the routers
    router = hr_router()
    router.link_bw = "4GB/s"
    router.flit_size = "8B"
    router.xbar_bw = "46GB/s"
    router.input_latency = "50ns"
    router.output_latency = "50ns"
    router.input_buf_size = "14kB"
    router.output_buf_size = "14kB"
    #router.num_vns = 1
    #router.xbar_arb = "merlin.xbar_arb_lru"

    topo.router = router
    topo.link_latency = "50ns"

    ### set up the endpoint
    networkif = ReorderLinkControl()
    networkif.link_bw = "4GB/s"
    networkif.input_buf_size = "14kB"
    networkif.output_buf_size = "14kB"

    networkif2 = ReorderLinkControl()
    networkif2.link_bw = "4GB/s"
    networkif2.input_buf_size = "14kB"
    networkif2.output_buf_size = "14kB"

    ep = SwmJob(2000,numNodesJob1)
    ep.network_interface = networkif
    ep.workload.numRanks=numRanksJob1
    ep.workload.name="incast"
    ep.workload.path="incast/incastTest.json"
    #ep.workload.name="lammps"
    #ep.workload.path="lammps/lammps_workload.json"
    ep.workload.verboseLevel=0
    ep.workload.verboseMask=-1
    #ep.nic.verboseLevel = 1
    ep.nic.verboseMask = (1<<3) | (1<<4) | ( 1<<7) | (1<<8) | (1<<11)
    #ep.nic.verboseMask = -1 & ( ~(1<<6) | ~(1<<10) )

    ep2 = EmptyJob(1,numNodesJob2)
    #ep2 = SwmJob(2001,topo.getNumNodes() // numJobs)
    #ep2.workload.numRanks=numNodes // numJobs
    #ep2.workload.name="lammps"
    #ep2.workload.path="lammps/lammps_workload.json"
    #ep2.workload.verboseLevel=0
    #ep2.workload.verboseMask=-1

    ep2.network_interface = networkif2

    system = System()
    system.setTopology(topo)
    system.allocateNodes(ep,"linear")
    system.allocateNodes(ep2,"linear")

    system.build()
