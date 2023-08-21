#!/usr/bin/env python3
# Copyright (c) 2015-2018 The Dash Core developers
# Copyright (c) 2023 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import HemisDGMTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
)
import time

'''
Check quorum based Chainlocks
'''


class ChainLocksTest(HemisDGMTestFramework):

    def set_test_params(self):
        self.set_base_test_params()
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=hemis_v5.5:130", "-nuparams=v6_evo:130", "-debug=llmq", "-debug=dkg", "-debug=net"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def run_test(self):
        miner = self.nodes[self.minerPos]

        # initialize and start gamemasters
        self.setup_test()
        assert_equal(len(self.gms), 6)

        # Mine a LLMQ final commitment regularly with 3 signers
        self.log.info("DKG Session Started")
        (qfc, badmembers) = self.mine_quorum()
        assert_equal(171, miner.getblockcount())

        # mine single block, wait for chainlock
        self.nodes[0].generate(1)
        self.wait_for_chainlock_tip_all_nodes()
        self.log.info("Chainlock successfully generated")
        # mine many blocks, wait for chainlock
        self.nodes[0].generate(20)
        self.wait_for_chainlock_tip_all_nodes()
        self.log.info("Chainlocks successfully generated again!")
        # assert that all blocks up until the tip are chainlocked
        for h in range(1, self.nodes[0].getblockcount()):
            block = self.nodes[0].getblock(self.nodes[0].getblockhash(h))
            assert (block['chainlock'])

        # Isolate node, mine on another, and reconnect
        self.nodes[0].setnetworkactive(False)
        node0_tip = self.nodes[0].getbestblockhash()
        self.nodes[1].generate(5)
        self.wait_for_chainlock_tip(self.nodes[1])
        assert (self.nodes[0].getbestblockhash() == node0_tip)
        self.nodes[0].setnetworkactive(True)
        connect_nodes(self.nodes[0], 1)
        self.nodes[1].generate(1)
        self.wait_for_chainlock(self.nodes[0], self.nodes[1].getbestblockhash())

        # Isolate node, mine on both parts of the network, and reconnect
        self.nodes[0].setnetworkactive(False)
        self.nodes[0].generate(5)
        self.nodes[1].generate(1)
        good_tip = self.nodes[1].getbestblockhash()
        self.wait_for_chainlock_tip(self.nodes[1])
        assert (not self.nodes[0].getblock(self.nodes[0].getbestblockhash())["chainlock"])
        self.nodes[0].setnetworkactive(True)
        connect_nodes(self.nodes[0], 1)
        self.nodes[1].generate(1)
        self.wait_for_chainlock(self.nodes[0], self.nodes[1].getbestblockhash())
        assert (self.nodes[0].getblock(self.nodes[0].getbestblockhash())["previousblockhash"] == good_tip)
        assert (self.nodes[1].getblock(self.nodes[1].getbestblockhash())["previousblockhash"] == good_tip)

        # Keep node connected and let it try to reorg the chain
        good_tip = self.nodes[0].getbestblockhash()
        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        # Restart it so that it forgets all the chainlocks from the past
        self.stop_node(0)
        self.start_node(0, extra_args=self.extra_args[0])
        connect_nodes(self.nodes[0], 1)
        # Now try to reorg the chain
        self.nodes[0].generate(2)
        time.sleep(2)
        assert (self.nodes[1].getbestblockhash() == good_tip)
        self.nodes[0].generate(2)
        time.sleep(2)
        assert (self.nodes[1].getbestblockhash() == good_tip)

        # Now let the node which is on the wrong chain reorg back to the locked chain
        self.nodes[0].reconsiderblock(good_tip)
        assert (self.nodes[0].getbestblockhash() != good_tip)
        self.nodes[1].generate(1)
        self.wait_for_chainlock(self.nodes[0], self.nodes[1].getbestblockhash())
        assert (self.nodes[0].getbestblockhash() == self.nodes[1].getbestblockhash())

    def wait_for_chainlock_tip_all_nodes(self):
        for node in self.nodes:
            tip = node.getbestblockhash()
            self.wait_for_chainlock(node, tip)

    def wait_for_chainlock_tip(self, node):
        tip = node.getbestblockhash()
        self.wait_for_chainlock(node, tip)

    def wait_for_chainlock(self, node, block_hash):
        t = time.time()
        while time.time() - t < 15:
            try:
                block = node.getblock(block_hash)
                if block["chainlock"]:
                    return
            except:
                # block might not be on the node yet
                pass
            time.sleep(0.1)
        raise AssertionError("wait_for_chainlock timed out")


if __name__ == '__main__':
    ChainLocksTest().main()
