#!/usr/bin/env python3
# Copyright (c) 2019-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import time

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    set_node_times,
)


def connect_nodes_bi(nodes, a, b):
    connect_nodes(nodes[a], b)
    connect_nodes(nodes[b], a)

class TimeOffsetTest(HemisTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 8
        self.enable_mocktime()

    def setup_network(self):
        # don't connect nodes yet
        self.setup_nodes()

    def check_connected_nodes(self):
        ni = [node.getnetworkinfo() for node in self.connected_nodes]
        assert_equal([x['connections'] for x in ni], [2] * len(ni))
        assert_equal([x['timeoffset'] for x in ni], [0] * len(ni))

    def run_test(self):
        # Nodes synced but not connected
        self.mocktime = int(time.time())
        set_node_times(self.nodes, self.mocktime)
        ni = [node.getnetworkinfo() for node in self.nodes]
        assert_equal([x['connections'] for x in ni], [0] * self.num_nodes)
        self.log.info("Nodes disconnected from each other. Time: %d" % self.mocktime)
        assert_equal([x['timeoffset'] for x in ni], [0] * self.num_nodes)
        self.log.info("Nodes have nTimeOffset 0")

        # Set node times.
        # nodes [1, 5]: set times to +10, +15, ..., +30 secs
        for i in range(1, 6):
            self.nodes[i].setmocktime(self.mocktime + 5 * (i + 1))
        # nodes [6, 7]: set time to -5, -10 secs
        for i in range(6, 8):
            self.nodes[i].setmocktime(self.mocktime - 5 * (i - 5))

        # connect nodes 1 and 2
        self.log.info("Connecting with node-1 (+10 s) and node-2 (+15 s)...")
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 0, 2)
        self.log.info("--> samples = [+0, +10, (+10), +15, +15]")
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 4)
        assert_equal(ni['timeoffset'], 10)
        self.connected_nodes = [self.nodes[1], self.nodes[2]]
        self.check_connected_nodes()
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])

        # connect node 3
        self.log.info("Connecting with node-3 (+20 s). This will print the warning...")
        connect_nodes_bi(self.nodes, 0, 3)
        self.log.info("--> samples = [+0, +10, +10, (+15), +15, +20, +20]")
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 6)
        assert_equal(ni['timeoffset'], 15)
        self.connected_nodes.append(self.nodes[3])
        self.check_connected_nodes()
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])

        # connect node 6
        self.log.info("Connecting with node-6 (-5 s)...")
        connect_nodes_bi(self.nodes, 0, 6)
        self.log.info("--> samples = [-5, -5, +0, +10, (+10), +15, +15, +20, +20]")
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 8)
        assert_equal(ni['timeoffset'], 10)
        self.connected_nodes.append(self.nodes[6])
        self.check_connected_nodes()
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])

        # connect node 4
        self.log.info("Connecting with node-4 (+25 s). This will print the warning...")
        connect_nodes_bi(self.nodes, 0, 4)
        self.log.info("--> samples = [-5, -5, +0, +10, +10, (+15), +15, +20, +20, +25, +25]")
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 10)
        assert_equal(ni['timeoffset'], 15)
        self.connected_nodes.append(self.nodes[4])
        self.check_connected_nodes()
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])

        # try to connect node 5 and check that it can't
        self.log.info("Trying to connect with node-5 (+30 s)...")
        connect_nodes_bi(self.nodes, 0, 5)
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 10)
        assert_equal(ni['timeoffset'], 15)
        self.log.info("Not connected.")
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])

        # connect node 7
        self.log.info("Connecting with node-7 (-10 s)...")
        connect_nodes_bi(self.nodes, 0, 7)
        self.log.info("--> samples = [-10, -10, -5, -5, +0, +10, (+10), +15, +15, +20, +20, +25, +25]")
        ni = self.nodes[0].getnetworkinfo()
        assert_equal(ni['connections'], 12)
        assert_equal(ni['timeoffset'], 10)
        self.connected_nodes.append(self.nodes[6])
        self.check_connected_nodes()
        self.log.info("Node-0 nTimeOffset: +%d seconds" % ni['timeoffset'])



if __name__ == '__main__':
    TimeOffsetTest().main()
