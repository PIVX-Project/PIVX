#!/usr/bin/env python3
# Copyright (c) 2023 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test a generic LLQM signing session"""

from test_framework.test_framework import HemisDGMTestFramework
from test_framework.util import (
    assert_equal,
)
import time

class SigningSessionTest(HemisDGMTestFramework):

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
        self.log.info("----------------------------------")
        self.log.info("----- (0) DKG session started -----")
        self.log.info("----------------------------------")
        (qfc, badmembers) = self.mine_quorum()
        assert_equal(171, miner.getblockcount())
        # Quorum members
        members = self.get_quorum_members(qfc['quorumHash'])

        # Let's begin with the signing session
        self.log.info("----------------------------------")
        self.log.info("----- (1) First signing session started -----")
        self.log.info("----------------------------------")
        time.sleep(5)

        # First scenario: ALL nodes agree on the msgHash of a given session-id
        id = "0000000000000000000000000000000000000000000000000000000000000001"
        msgHash = "0000000000000000000000000000000000000000000000000000000000000002"
        msgHashConflict = "0000000000000000000000000000000000000000000000000000000000000003"
        self.log.info("starting signing")
        # Only valid quorum memebrs can sign the message
        for i in range(len(self.nodes)):
            if (i in [m.idx for m in members]):
                assert_equal(True, self.nodes[i].signsession(100, id, msgHash))
            else:
                assert_equal(False, self.nodes[i].signsession(100, id, msgHash))
        self.log.info("finished signing")
        # At this point a recovery trheshold signaure should have been generated and propagated to the whole network
        # Let's generate some blocks to ensure that nodes are synced
        time.sleep(5)
        for i in range(len(self.nodes)):
            assert_equal(True, self.nodes[i].hasrecoverysignature(100, id, msgHash))

        # Moreover since we have a recovery signature we surely cannot sign a message with the same id
        for i in range(len(self.nodes)):
            assert_equal(False, self.nodes[i].hasrecoverysignature(100, id, msgHashConflict))

        self.log.info("Threshold signature succesfully generated and propagated!")

        # Second scenario, let's select a new signing session (i.e. a new id) and this time nodes will not agree on the msgHash
        self.log.info("----------------------------------")
        self.log.info("----- (2) Second signing session started -----")
        self.log.info("----------------------------------")
        id = "0000000000000000000000000000000000000000000000000000000000000002"
        msgHash = "0000000000000000000000000000000000000000000000000000000000000002"
        msgHashConflict = "0000000000000000000000000000000000000000000000000000000000000003"

        firstSigner = True

        # This time a 2 signers will agree on msgHash while the other in msgHasConflict
        for i in range(len(self.nodes)):
            if (i in [m.idx for m in members]):
                if firstSigner:
                    assert_equal(True, self.nodes[i].signsession(100, id, msgHashConflict))
                    firstSigner = False
                else:
                    assert_equal(True, self.nodes[i].signsession(100, id, msgHash))
            else:
                assert_equal(False, self.nodes[i].signsession(100, id, msgHash))

        # Since with this quorum type 2 nodes are enough to generate the treshold signature at the end every node MUST agree on (id, msgHash)
        # Let's wait a bit to sync all messages
        time.sleep(5)
        for i in range(len(self.nodes)):
            assert_equal(True, self.nodes[i].hasrecoverysignature(100, id, msgHash))
        self.log.info("Threshold signature succesfully generated and propagated!")

        # Finally let's test that those signatures are valid in the future:
        # it must be valid if we generate enough quorums to push the first one out of the active set
        self.mine_quorum()
        self.mine_quorum()
        time.sleep(5)
        for i in range(len(self.nodes)):
            assert_equal(True, self.nodes[i].hasrecoverysignature(100, id, msgHash))
        self.log.info("Threshold signature is still valid after the corresponding quorum went inactive!")


if __name__ == '__main__':
    SigningSessionTest().main()
