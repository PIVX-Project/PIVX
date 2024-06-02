#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Copyright (c) 2023 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

"""Test running pivxd with -rewindblockindex option.

- Start 2 Nodes
- Sync both nodes to different chains
- Stop and then start both nodes with -rewindblockindex

"""

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
        assert_equal,
        connect_nodes,
)

class RewindBlockIndexCheckpointTest(PivxTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [
                ['-nuparams=PoS:201', '-nuparams=PoS_v2:201', "-whitelist=127.0.0.1", '-debug=0'], #right chain
                ['-nuparams=PoS:301', '-nuparams=PoS_v2:301', "-whitelist=127.0.0.1", '-debug=0']  #wrong chain
                ]

    def run_test(self):
        self.log.info("Starting nodes")

        res0 = self.nodes[0].getblockchaininfo()
        res1 = self.nodes[1].getblockchaininfo()

        self.nodes[0].generate(1)
        self.nodes[1].generate(1)

        res0 = self.nodes[0].getblockchaininfo()
        res1 = self.nodes[1].getblockchaininfo()

        rightChain = res0["bestblockhash"]
        wrongChain = res1["bestblockhash"]

        assert not rightChain==wrongChain

        self.log.info("Restart Node1 (after upgrade window) with forced rewindblockindex")
        # restart node1 with right nuparams and rewindblockindex
        # on regtest only forced rewinds possible because there are no checkpoints
        self.restart_node(1, extra_args=['-nuparams=PoS:201', '-nuparams=PoS_v2:201', '-rewindblockindex', '-debug=0'])

        res0 = self.nodes[0].getblockchaininfo()
        res1 = self.nodes[1].getblockchaininfo()

        connect_nodes(self.nodes[0], 1)
        self.sync_blocks([self.nodes[i] for i in [0, 1]], timeout=120)

        res0 = self.nodes[0].getblockchaininfo()
        res1 = self.nodes[1].getblockchaininfo()

        assert_equal(res1["bestblockhash"], rightChain)

        self.log.info("Node1 on right chain")

if __name__ == '__main__':
    RewindBlockIndexCheckpointTest().main()
