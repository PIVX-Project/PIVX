#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test errors during DKG phases"""

from test_framework.test_framework import PivxDMNTestFramework, ExpectedDKGMessages
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class DkgErrorsTest(PivxDMNTestFramework):

    def set_test_params(self):
        self.set_base_test_params()
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=hemis_v5.5:130", "-nuparams=v6_evo:130", "-debug=llmq", "-debug=dkg", "-debug=net"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def reset_simerror(self, node):
        node.quorumdkgsimerror("contribution-omit", 0)
        node.quorumdkgsimerror("contribution-lie", 0)
        node.quorumdkgsimerror("complain-lie", 0)
        node.quorumdkgsimerror("justify-lie", 0)
        node.quorumdkgsimerror("justify-omit", 0)
        node.quorumdkgsimerror("commit-omit", 0)
        node.quorumdkgsimerror("commit-lie", 0)

    def no_contrib(self, node):
        node.quorumdkgsimerror("contribution-omit", 1)

    def invalid_contrib(self, node):
        node.quorumdkgsimerror("contribution-lie", 1)

    def invalid_contrib_no_justif(self, node):
        node.quorumdkgsimerror("contribution-lie", 1)
        node.quorumdkgsimerror("justify-omit", 1)

    def invalid_contrib_invalid_justif(self, node):
        node.quorumdkgsimerror("contribution-lie", 1)
        node.quorumdkgsimerror("justify-lie", 1)

    def invalid_complaint(self, node):
        node.quorumdkgsimerror("complain-lie", 1)

    def no_commit(self, node):
        node.quorumdkgsimerror("commit-omit", 1)

    def invalid_commit(self, node):
        node.quorumdkgsimerror("commit-lie", 1)

    def repair_mn(self, mnode, blocks=None):
        if blocks is None:
            blocks = self.nodes[0].listmasternodes(mnode.proTx)[0]["dmnstate"]["PoSePenalty"]
        self.log.info("Mining %d blocks to repair MN (node %d)..." % (blocks, mnode.idx))
        self.nodes[0].generate(blocks)
        self.sync_blocks()

    def is_punished(self, proTx, expected_penalty):
        return self.nodes[0].listmasternodes(proTx)[0]["dmnstate"]["PoSePenalty"] == expected_penalty

    def is_not_punished(self, proTx):
        return self.nodes[0].listmasternodes(proTx)[0]["dmnstate"]["PoSePenalty"] == 0

    def run_test(self):
        miner = self.nodes[self.minerPos]

        # initialize and start masternodes
        self.setup_test()
        assert_equal(len(self.mns), 6)

        # Mine a LLMQ final commitment regularly with 3 signers
        self.log.info("----------------------------------")
        self.log.info("----- (0) DKG with no errors -----")
        self.log.info("----------------------------------")
        self.mine_quorum()
        assert_equal(171, miner.getblockcount())

        # RPC: quorumdkgsimerror - invalid parameters
        self.log.info("Trying invalid parameters for quorumdkgsimerror")
        assert_raises_rpc_error(-8, "invalid rate. Must be between 0 and 1",
                                miner.quorumdkgsimerror, "contribution-omit", -0.01)
        assert_raises_rpc_error(-8, "invalid rate. Must be between 0 and 1",
                                miner.quorumdkgsimerror, "contribution-omit", 1.01)
        assert_raises_rpc_error(-8, "invalid error_type: foo",
                                miner.quorumdkgsimerror, "foo", 0.5)

        self.log.info("-------------------------------------")
        self.log.info("----- (1) Omit the contribution -----")
        self.log.info("-------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=(i != 2), s_complaint=True, s_justif=False, s_commit=True,
                                        r_contrib=2, r_complaint=3, r_justif=0, r_commit=3) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.no_contrib,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 0], signers=[1, 1, 1])
        assert self.is_punished(bad_mnode.proTx, 66)
        self.log.info("Node %d punished." % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])
        self.repair_mn(bad_mnode, 66)

        self.log.info("-------------------------------------------------------------")
        self.log.info("----- (2) Lie on contribution but provide justification -----")
        self.log.info("-------------------------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=(i != 2), s_justif=(i == 2), s_commit=True,
                                        r_contrib=3, r_complaint=2, r_justif=1, r_commit=3) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.invalid_contrib,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 1], signers=[1, 1, 1])
        assert self.is_not_punished(bad_mnode.proTx)
        self.log.info("Node %d justified." % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])

        self.log.info("----------------------------------------------------------")
        self.log.info("----- (3) Lie on contribution and omit justification -----")
        self.log.info("----------------------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=(i != 2), s_justif=False, s_commit=True,
                                        r_contrib=3, r_complaint=2, r_justif=0, r_commit=3) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.invalid_contrib_no_justif,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 0], signers=[1, 1, 1])
        assert self.is_punished(bad_mnode.proTx, 66)
        self.log.info("Node %d punished." % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])
        self.repair_mn(bad_mnode, 66)

        self.log.info("------------------------------------------------------------")
        self.log.info("----- (4) Lie on contribution and lie on justification -----")
        self.log.info("------------------------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=(i != 2), s_justif=(i == 2), s_commit=True,
                                        r_contrib=3, r_complaint=2, r_justif=1, r_commit=3) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.invalid_contrib_invalid_justif,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 0], signers=[1, 1, 1])
        assert self.is_punished(bad_mnode.proTx, 66)
        self.log.info("Node %d punished." % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])
        self.repair_mn(bad_mnode, 66)

        self.log.info("---------------------------------------------------")
        self.log.info("----- (5) Lie on complaints for other members -----")
        self.log.info("---------------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=(i == 2), s_justif=(i != 2), s_commit=True,
                                        r_contrib=3, r_complaint=1, r_justif=2, r_commit=3) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.invalid_complaint,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 1], signers=[1, 1, 1])
        assert self.is_not_punished(bad_mnode.proTx)
        self.log.info("Invalid complaint from %d justified." % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])

        self.log.info("---------------------------------------------")
        self.log.info("----- (6) Omit the premature commitment -----")
        self.log.info("---------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=False, s_justif=False, s_commit=(i != 2),
                                        r_contrib=3, r_complaint=0, r_justif=0, r_commit=2) for i in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.no_commit,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 1], signers=[1, 1, 0])
        assert self.is_not_punished(bad_mnode.proTx)
        self.log.info("Mined final commitment without node %d signature" % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])

        self.log.info("-------------------------------------------")
        self.log.info("----- (7) Lie on premature commitment -----")
        self.log.info("-------------------------------------------")
        dkg_msgs = [ExpectedDKGMessages(s_contrib=True, s_complaint=False, s_justif=False, s_commit=True,
                                        r_contrib=3, r_complaint=0, r_justif=0, r_commit=2) for _ in range(3)]
        qfc, bad_mnode = self.mine_quorum(invalidate_func=self.invalid_commit,
                                          expected_messages=dkg_msgs)
        self.check_final_commitment(qfc, valid=[1, 1, 1], signers=[1, 1, 0])
        assert self.is_not_punished(bad_mnode.proTx)
        self.log.info("Mined final commitment without node %d signature" % bad_mnode.idx)
        self.reset_simerror(self.nodes[bad_mnode.idx])


if __name__ == '__main__':
    DkgErrorsTest().main()
