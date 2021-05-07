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

    ### Setup the topology
    topo = topoDragonFly()
    topo.hosts_per_router = 2
    topo.routers_per_group = 4
    topo.intergroup_links = 2
    topo.num_groups = 4
    topo.algorithm = ["minimal","adaptive-local"]
    
    # Set up the routers
    router = hr_router()
    router.link_bw = "4GB/s"
    router.flit_size = "8B"
    router.xbar_bw = "4GB/s"
    router.input_latency = "50ns"
    router.output_latency = "50ns"
    router.input_buf_size = "14kB"
    router.output_buf_size = "14kB"
    router.num_vns = 2
    router.xbar_arb = "merlin.xbar_arb_lru"

    topo.router = router
    topo.link_latency = "40ns"
    
    ### set up the endpoint
    networkif = ReorderLinkControl()
    networkif.link_bw = "4GB/s"
    networkif.input_buf_size = "14kB"
    networkif.output_buf_size = "14kB"

    ep = SwmJob(0,topo.getNumNodes())
    ep.network_interface = networkif
    ep.workload.numRanks=32
    ep.workload.name="lammps"   
    ep.workload.path="lammps/lammps_workload.json"
    ep.workload.verboseLevel = 0 
    ep.workload.verboseMask = 1<<0

    ep.nic.nic2host_lat= "200ns"
        
    system = System()
    system.setTopology(topo)
    system.allocateNodes(ep,"linear")

    system.build()
