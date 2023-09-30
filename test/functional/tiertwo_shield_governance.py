#!/usr/bin/env python3
# Copyright (c) 2023 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
This (temporary) test checks the new vote system for SDMNs and DMNs.
Once legacy are obsolete this test will be merged with tiertwo_governance_sync_basic,
where we can simply change the two legacy MNs in SDMNs.
"""

import time

from test_framework.test_framework import PivxTestFramework
from test_framework.budget_util import (
    check_budget_finalization_sync,
    create_proposals_tx,
    check_budgetprojection,
    check_proposal_existence,
    check_mns_status,
    check_vote_existence,
    get_proposal_obj,
    Proposal,
    propagate_proposals
)
from test_framework.util import (
    assert_equal,
    satoshi_round,
    set_node_times
)


class SDMNsGovernanceTest(PivxTestFramework):

    def wait_until_mnsync_completed(self):
        SYNC_FINISHED = [999] * self.num_nodes
        synced = [-1] * self.num_nodes
        timeout = time.time() + 120
        while synced != SYNC_FINISHED and time.time() < timeout:
            synced = [node.mnsync("status")["RequestedMasternodeAssets"]
                      for node in self.nodes]
            if synced != SYNC_FINISHED:
                time.sleep(5)
        if synced != SYNC_FINISHED:
            raise AssertionError("Unable to complete mnsync: %s" % str(synced))

    def broadcastbudgetfinalization(self):
        miner = self.nodes[0]
        self.log.info("suggesting the budget finalization..")
        assert miner.mnfinalbudgetsuggest() is not None

        self.log.info("confirming the budget finalization..")
        time.sleep(1)
        miner.generate(4)
        self.sync_blocks()

        self.log.info("broadcasting the budget finalization..")
        return miner.mnfinalbudgetsuggest()

    def submit_proposals(self, props):
        miner = self.nodes[0]
        props = create_proposals_tx(miner, props)
        # generate 3 blocks to confirm the tx (and update the mnping)
        miner.generate(3)
        self.sync_blocks()
        # check fee tx existence
        for entry in props:
            txinfo = miner.gettransaction(entry.feeTxId)
            assert_equal(txinfo['amount'], -50.00)
        # propagate proposals
        props = propagate_proposals(miner, props)
        # let's wait a little bit and see if all nodes are sync
        time.sleep(1)
        for entry in props:
            check_proposal_existence(self.nodes, entry.name, entry.proposalHash)
            self.log.info("proposal %s broadcast successful!" % entry.name)
        return props

    def set_test_params(self):
        # 1 miner, 1 controller, 1 DMNs and 1 SDMN
        self.num_nodes = 4
        self.minerPos = 0
        self.controllerPos = 1
        self.setup_clean_chain = True
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:130"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def run_test(self):
        # Additional connections to miner and owner
        for nodePos in [self.minerPos, self.controllerPos]:
            self.connect_to_all(nodePos)
        miner = self.nodes[self.minerPos]
        controller = self.nodes[self.controllerPos]

        # Enforce mn payments and reject legacy mns at block 131
        self.activate_spork(self.minerPos, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_13_ENABLE_SUPERBLOCKS")
        assert_equal("success", self.set_spork(self.minerPos, "SPORK_21_LEGACY_MNS_MAX_HEIGHT", 130))
        time.sleep(1)
        assert_equal([130] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_MNS_MAX_HEIGHT")
                                              for x in range(self.num_nodes)])
        mns = []

        # DIP3 activates at block 130 but we mine until block 145 (so we are over the first budget payment).
        miner.generate(145 - miner.getblockcount())
        self.sync_blocks()
        self.assert_equal_for_all(145, "getblockcount")
        self.wait_until_mnsync_completed()

        # Create 1 SDMN and 1 DMN:
        self.log.info("Initializing masternodes...")
        mns.append(self.register_new_dmn(2, self.minerPos, self.controllerPos, "internal", False))
        mns.append(self.register_new_dmn(3, self.minerPos, self.controllerPos, "internal", True))

        for mn in mns:
            self.nodes[mn.idx].initmasternode(mn.operator_sk)
            time.sleep(1)
        miner.generate(1)
        self.sync_blocks()

        for mn in mns:
            check_mns_status(self.nodes[mn.idx], mn.proTx)
        self.log.info("All masternodes activated")

        nextSuperBlockHeight = miner.getnextsuperblock()

        # Submit first proposal
        self.log.info("preparing budget proposal..")
        firstProposal = Proposal(
            "sdmns-are-the-best",
            "https://forum.pivx.org/t/fund-my-proposal",
            2,
            miner.getnewaddress(),
            300
        )
        self.submit_proposals([firstProposal])

        # Check proposals existence
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 1)

        # now let's vote for the proposal with the first DMN
        self.log.info("Voting with DMN1...")
        voteResult = controller.mnbudgetvote("alias", firstProposal.proposalHash, "yes", mns[1].proTx)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        miner.generate(1)
        self.sync_blocks()
        check_vote_existence(self.nodes, firstProposal.name, mns[1].proTx, "YES", True)
        self.log.info("all good, DMN1 vote accepted everywhere!")

        # now let's vote for the proposal with the first SDMN
        self.log.info("Voting with SDMN1...")
        voteResult = controller.mnbudgetvote("alias", firstProposal.proposalHash, "yes", mns[0].proTx)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        miner.generate(1)
        self.sync_blocks()
        check_vote_existence(self.nodes, firstProposal.name, mns[0].proTx, "YES", True)
        self.log.info("all good, SDMN1 vote accepted everywhere!")

        # instead of waiting for 5 minutes for proposal to be established advance the nodes time of 10 minutes
        set_node_times(self.nodes, int(time.time()) + 10*60)

        # check budgets
        blockStart = nextSuperBlockHeight
        blockEnd = blockStart + firstProposal.cycles * 145
        TotalPayment = firstProposal.amountPerCycle * firstProposal.cycles
        Allotted = firstProposal.amountPerCycle
        RemainingPaymentCount = firstProposal.cycles
        expected_budget = [
            get_proposal_obj(firstProposal.name, firstProposal.link, firstProposal.proposalHash, firstProposal.feeTxId, blockStart,
                                  blockEnd, firstProposal.cycles, RemainingPaymentCount, firstProposal.paymentAddr, 1,
                                  2, 0, 0, satoshi_round(TotalPayment), satoshi_round(firstProposal.amountPerCycle),
                                  True, True, satoshi_round(Allotted), satoshi_round(Allotted))
                           ]
        check_budgetprojection(self.nodes, expected_budget, self.log)

        # Mine more block in order to finalize the budget.
        while (miner.getblockcount() < 279):
            miner.generate(1)
        self.sync_blocks()
        assert_equal(controller.getblockcount(), 279)

        self.log.info("starting budget finalization sync test..")
        miner.generate(2)
        self.sync_blocks()

        # assert that there is no budget finalization first.
        assert_equal(len(controller.mnfinalbudget("show")), 0)

        # suggest the budget finalization and confirm the tx (+4 blocks).
        budgetFinHash = self.broadcastbudgetfinalization()
        assert budgetFinHash != ""
        time.sleep(2)

        self.log.info("checking budget finalization sync..")
        check_budget_finalization_sync(self.nodes, 0, "OK")

        self.log.info("budget finalization synced!, now voting for the budget finalization..")
        for mn in mns:
            voteResult = self.nodes[mn.idx].mnfinalbudget("vote", budgetFinHash)
            assert_equal(voteResult["detail"][0]["result"], "success")
            miner.generate(2)
            self.sync_blocks()
        check_budget_finalization_sync(self.nodes, 2, "OK")
        self.log.info("Both masternodes voted successfully.")

        miner.generate(6)
        self.sync_blocks()
        addrInfo = miner.listreceivedbyaddress(0, False, False, firstProposal.paymentAddr)
        assert_equal(addrInfo[0]["amount"], firstProposal.amountPerCycle)

        self.log.info("budget proposal paid!, all good")

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        check_budgetprojection(self.nodes, expected_budget, self.log)


if __name__ == '__main__':
    SDMNsGovernanceTest().main()
