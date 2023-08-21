#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test DKG and PoSe ban of missing nodes"""

import time

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import HemisDGMTestFramework, ExpectedDKGMessages
from test_framework.util import (
    assert_equal,
    wait_until,
)


class DkgPoseTest(HemisDGMTestFramework):

    def set_test_params(self):
        self.set_base_test_params()
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:130", "-debug=llmq", "-debug=dkg", "-debug=net"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def disable_network_for_node(self, node):
        node.setnetworkactive(False)
        wait_until(lambda: node.getconnectioncount() == 0)

    def wait_for_spork22(self, fEnabled):
        time.sleep(1)
        for i in range(self.num_nodes):
            wait_until(lambda: self.is_spork_active(i, "SPORK_22_LLMQ_DKG_MAINTENANCE") == fEnabled, timeout=5)

    def set_spork_22(self, fEnabled):
        if fEnabled:
            self.activate_spork(self.minerPos, "SPORK_22_LLMQ_DKG_MAINTENANCE")
        else:
            self.deactivate_spork(self.minerPos, "SPORK_22_LLMQ_DKG_MAINTENANCE")
        self.wait_for_spork22(fEnabled)

    def run_test(self):
        miner = self.nodes[self.minerPos]

        # initialize and start gamemasters
        self.setup_test()
        assert_equal(len(self.gms), 6)

        # Set DKG maintenance (SPORK 22) and verify that the mined commitment is null
        self.log.info("Activating SPORK 22...")
        self.set_spork_22(True)
        try:
            self.mine_quorum()
            raise Exception("Non-null commitment mined with SPORK 22")
        except JSONRPCException as e:
            assert_equal(e.error["code"], -8)
            assert_equal(e.error["message"], "mined commitment not found")
            self.log.info("Null commitment mined")

        self.log.info("Deactivating SPORK 22...")
        self.set_spork_22(False)

        # Mine a LLMQ final commitment regularly with 3 signers
        qfc, bad_gmode = self.mine_quorum()
        assert_equal(191, miner.getblockcount())
        assert bad_gmode is None
        self.check_final_commitment(qfc, valid=[1, 1, 1], signers=[1, 1, 1])

        # Mine the next final commitment after disconnecting a member
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=True, s_justif=False, s_commit=True,
                                        r_contrib=2, r_complaint=2, r_justif=0, r_commit=2) for _ in range(2)]
        qfc, bad_gmode = self.mine_quorum(invalidate_func=self.disable_network_for_node,
                                          skip_bad_member_sync=True, expected_messages=dkg_msgs)
        assert_equal(211, miner.getblockcount())
        assert bad_gmode is not None
        self.check_final_commitment(qfc, valid=[1, 1, 0], signers=[1, 1, 0])

        # Check PoSe
        self.log.info("Check that node %d has been PoSe punished..." % bad_gmode.idx)
        expected_penalty = 66
        assert_equal(expected_penalty, miner.listgamemasters(bad_gmode.proTx)[0]["dgmstate"]["PoSePenalty"])
        self.log.info("Node punished.")
        # penalty decreases at every block
        miner.generate(1)
        self.sync_all([n for i, n in enumerate(self.nodes) if i != bad_gmode.idx])
        expected_penalty -= 1
        assert_equal(expected_penalty, miner.listgamemasters(bad_gmode.proTx)[0]["dgmstate"]["PoSePenalty"])

        # Keep mining commitments until the bad node is banned
        timeout = time.time() + 600
        while time.time() < timeout:
            self.mine_quorum(invalidated_idx=bad_gmode.idx, skip_bad_member_sync=True, expected_messages=dkg_msgs)
            pose_height = miner.listgamemasters(bad_gmode.proTx)[0]["dgmstate"]["PoSeBanHeight"]
            if pose_height != -1:
                self.log.info("Node %d has been PoSeBanned at height %d" % (bad_gmode.idx, pose_height))
                self.log.info("All good.")
                return
            time.sleep(1)
        # timeout
        raise Exception("Node not banned after 10 minutes")

if __name__ == '__main__':
    DkgPoseTest().main()
