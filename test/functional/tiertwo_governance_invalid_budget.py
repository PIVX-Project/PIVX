#!/usr/bin/env python3
# Copyright (c) 2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    p2p_port,
)

import os
import time

class GovernanceInvalidBudgetTest(HemisTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        # 3 nodes:
        # - 1 miner/gmcontroller
        # - 2 remote gms
        self.num_nodes = 3
        self.extra_args = [["-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi"],
                           [],
                           [],
                           ]
        self.enable_mocktime()

        self.minerAPos = 0
        self.remoteOnePos = 1
        self.remoteTwoPos = 2

        self.gamemasterOneAlias = "gmOne"
        self.gamemasterTwoAlias = "gmtwo"

        self.gmOnePrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"
        self.gmTwoPrivkey = "92Hkebp3RHdDidGZ7ARgS4orxJAGyFUPDXNqtsYsiwho1HGVRbF"

    def run_test(self):
        self.minerA = self.nodes[self.minerAPos]     # also controller of gm1 and gm2
        self.gm1 = self.nodes[self.remoteOnePos]
        self.gm2 = self.nodes[self.remoteTwoPos]
        self.setupContext()

        # Create a valid proposal and vote on it
        next_superblock = self.minerA.getnextsuperblock()
        payee = self.minerA.getnewaddress()
        self.log.info("Creating a proposal to be paid at block %d" % next_superblock)
        proposalFeeTxId = self.minerA.preparebudget("test1", "https://test1.org", 2,
                                               next_superblock, payee, 300)
        self.stake_and_ping(self.minerAPos, 3, [self.gm1, self.gm2])
        proposalHash = self.minerA.submitbudget("test1", "https://test1.org", 2,
                                           next_superblock, payee, 300, proposalFeeTxId)
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 7, [self.gm1, self.gm2])
        self.log.info("Vote for the proposal and check projection...")
        self.minerA.gmbudgetvote("alias", proposalHash, "yes", self.gamemasterOneAlias)
        self.minerA.gmbudgetvote("alias", proposalHash, "yes", self.gamemasterTwoAlias)
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 1, [self.gm1, self.gm2])
        projection = self.gm1.getbudgetprojection()[0]
        assert_equal(projection["Name"], "test1")
        assert_equal(projection["Hash"], proposalHash)
        assert_equal(projection["Yeas"], 2)

        # Try to create an invalid finalized budget, paying to an nonexistent proposal
        self.log.info("Creating invalid budget finalization...")
        self.stake_and_ping(self.minerAPos, 5, [self.gm1, self.gm2])

        budgetname = "invalid finalization"
        blockstart = self.minerA.getnextsuperblock()
        proposals = []
        badPropId = "aa0061d705de36385c37701e7632408bd9d2876626b1299a17f7dc818c0ad285"
        badPropPayee = "8c988f1a4a4de2161e0f50aac7f17e7f9555caa4"
        badPropAmount = 500
        proposals.append({"proposalid": badPropId, "payee": badPropPayee, "amount": badPropAmount})
        res = self.minerA.createrawgmfinalbudget(budgetname, blockstart, proposals)
        assert(res["result"] == "tx_fee_sent")
        feeBudgetId = res["id"]
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 4, [self.gm1, self.gm2])
        res = self.minerA.createrawgmfinalbudget(budgetname, blockstart, proposals, feeBudgetId)
        assert(res["result"] == "error") # not accepted

        self.log.info("Good, invalid budget not accepted.")

    def send_3_pings(self, gm_list):
        self.advance_mocktime(30)
        self.send_pings(gm_list)
        self.stake_and_ping(self.minerAPos, 1, gm_list)
        self.advance_mocktime(30)
        self.send_pings(gm_list)
        time.sleep(2)

    def setupContext(self):
        # First mine 250 PoW blocks (250 with minerA)
        self.log.info("Generating 259 blocks...")
        for _ in range(250):
            self.mocktime = self.generate_pow(self.minerAPos, self.mocktime)
        self.sync_blocks()
        # Then stake 9 blocks with minerA
        self.stake_and_ping(self.minerAPos, 9, [])
        for n in self.nodes:
            assert_equal(n.getblockcount(), 259)

        # Setup Gamemasters
        self.log.info("Gamemasters setup...")
        ownerdir = os.path.join(self.options.tmpdir, "node%d" % self.minerAPos, "regtest")
        self.gmOneCollateral = self.setupGamemaster(self.minerA, self.minerA, self.gamemasterOneAlias,
                                                    ownerdir, self.remoteOnePos, self.gmOnePrivkey)
        self.gmTwoCollateral = self.setupGamemaster(self.minerA, self.minerA, self.gamemasterTwoAlias,
                                                    ownerdir, self.remoteTwoPos, self.gmTwoPrivkey)

        # Activate gamemasters
        self.log.info("Gamemasters activation...")
        self.stake_and_ping(self.minerAPos, 1, [])
        time.sleep(3)
        self.advance_mocktime(10)
        remoteOnePort = p2p_port(self.remoteOnePos)
        remoteTwoPort = p2p_port(self.remoteTwoPos)
        self.gm1.initgamemaster(self.gmOnePrivkey, "127.0.0.1:"+str(remoteOnePort))
        self.gm2.initgamemaster(self.gmTwoPrivkey, "127.0.0.1:"+str(remoteTwoPort))
        self.stake_and_ping(self.minerAPos, 1, [])
        self.wait_until_gmsync_finished()
        self.controller_start_gamemaster(self.minerA, self.gamemasterOneAlias)
        self.controller_start_gamemaster(self.minerA, self.gamemasterTwoAlias)
        self.wait_until_gm_preenabled(self.gmOneCollateral.hash, 40)
        self.wait_until_gm_preenabled(self.gmOneCollateral.hash, 40)
        self.send_3_pings([self.gm1, self.gm2])
        self.wait_until_gm_enabled(self.gmOneCollateral.hash, 120, [self.gm1, self.gm2])
        self.wait_until_gm_enabled(self.gmOneCollateral.hash, 120, [self.gm1, self.gm2])

        # activate sporks
        self.log.info("Gamemasters enabled. Activating sporks.")
        self.activate_spork(self.minerAPos, "SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerAPos, "SPORK_9_GAMEMASTER_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerAPos, "SPORK_13_ENABLE_SUPERBLOCKS")


if __name__ == '__main__':
    GovernanceInvalidBudgetTest().main()
