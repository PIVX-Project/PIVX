#!/usr/bin/env python3
# Copyright (c) 2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking:
 1) Masternodes setup/creation.
 2) Proposal creation.
 3) Vote creation.
 4) Proposal and vote broadcast.
 5) Proposal and vote sync.
"""

from test_framework.messages import COutPoint
from test_framework.test_framework import PivxTier2TestFramework
from test_framework.util import (
    assert_equal,
    assert_true,
    satoshi_round,
)

import time

class MasternodeGovernanceBasicTest(PivxTier2TestFramework):

    def check_mns_status_legacy(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["txhash"], txhash)
        assert_equal(status["message"], "Masternode successfully started")

    def check_mns_status(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["proTxHash"], txhash)
        assert_equal(status["dmnstate"]["PoSePenalty"], 0)
        assert_equal(status["status"], "Ready")

    def check_mn_list(self, node, txHashSet):
        # check masternode list from node
        mnlist = node.listmasternodes()
        assert_equal(len(mnlist), 3)
        foundHashes = set([mn["txhash"] for mn in mnlist if mn["txhash"] in txHashSet])
        assert_equal(len(foundHashes), len(txHashSet))

    def check_budget_finalization_sync(self, votesCount, status):
        for i in range(0, len(self.nodes)):
            node = self.nodes[i]
            budFin = node.mnfinalbudget("show")
            assert_true(len(budFin) == 1, "MN budget finalization not synced in node" + str(i))
            budget = budFin[next(iter(budFin))]
            assert_equal(budget["VoteCount"], votesCount)
            assert_equal(budget["Status"], status)

    def broadcastbudgetfinalization(self, node, with_ping_mns=[]):
        self.log.info("suggesting the budget finalization..")
        assert (node.mnfinalbudgetsuggest() is not None)

        self.log.info("confirming the budget finalization..")
        time.sleep(1)
        self.stake(4, with_ping_mns)

        self.log.info("broadcasting the budget finalization..")
        return node.mnfinalbudgetsuggest()

    def check_proposal_existence(self, proposalName, proposalHash):
        for node in self.nodes:
            proposals = node.getbudgetinfo(proposalName)
            assert(len(proposals) > 0)
            assert_equal(proposals[0]["Hash"], proposalHash)

    def check_vote_existence(self, proposalName, mnCollateralHash, voteType, voteValid):
        for i in range(0, len(self.nodes)):
            node = self.nodes[i]
            votesInfo = node.getbudgetvotes(proposalName)
            assert(len(votesInfo) > 0)
            found = False
            for voteInfo in votesInfo:
                if (voteInfo["mnId"].split("-")[0] == mnCollateralHash) :
                    assert_equal(voteInfo["Vote"], voteType)
                    assert_equal(voteInfo["fValid"], voteValid)
                    found = True
            assert_true(found, "Error checking vote existence in node " + str(i))

    def get_proposal_obj(self, Name, URL, Hash, FeeHash, BlockStart, BlockEnd,
                             TotalPaymentCount, RemainingPaymentCount, PaymentAddress,
                             Ratio, Yeas, Nays, Abstains, TotalPayment, MonthlyPayment,
                             IsEstablished, IsValid, Allotted, TotalBudgetAllotted, IsInvalidReason = ""):
        obj = {}
        obj["Name"] = Name
        obj["URL"] = URL
        obj["Hash"] = Hash
        obj["FeeHash"] = FeeHash
        obj["BlockStart"] = BlockStart
        obj["BlockEnd"] = BlockEnd
        obj["TotalPaymentCount"] = TotalPaymentCount
        obj["RemainingPaymentCount"] = RemainingPaymentCount
        obj["PaymentAddress"] = PaymentAddress
        obj["Ratio"] = Ratio
        obj["Yeas"] = Yeas
        obj["Nays"] = Nays
        obj["Abstains"] = Abstains
        obj["TotalPayment"] = TotalPayment
        obj["MonthlyPayment"] = MonthlyPayment
        obj["IsEstablished"] = IsEstablished
        obj["IsValid"] = IsValid
        if IsInvalidReason != "":
            obj["IsInvalidReason"] = IsInvalidReason
        obj["Allotted"] = Allotted
        obj["TotalBudgetAllotted"] = TotalBudgetAllotted
        return obj

    def check_budgetprojection(self, expected):
        for i in range(self.num_nodes):
            assert_equal(self.nodes[i].getbudgetprojection(), expected)
            self.log.info("Budget projection valid for node %d" % i)

    def run_test(self):
        self.enable_mocktime()
        self.setup_3_masternodes_network()
        txHashSet = set([self.mnOneCollateral.hash, self.mnTwoCollateral.hash, self.proRegTx1])
        # check mn list from miner
        self.check_mn_list(self.miner, txHashSet)

        # check status of masternodes
        self.check_mns_status_legacy(self.remoteOne, self.mnOneCollateral.hash)
        self.log.info("MN1 active")
        self.check_mns_status_legacy(self.remoteTwo, self.mnTwoCollateral.hash)
        self.log.info("MN2 active")
        self.check_mns_status(self.remoteDMN1, self.proRegTx1)
        self.log.info("DMN1 active")

        # Prepare the proposal
        self.log.info("preparing budget proposal..")
        firstProposalName = "super-cool"
        firstProposalLink = "https://forum.pivx.org/t/test-proposal"
        firstProposalCycles = 2
        firstProposalAddress = self.miner.getnewaddress()
        firstProposalAmountPerCycle = 300
        nextSuperBlockHeight = self.miner.getnextsuperblock()

        proposalFeeTxId = self.miner.preparebudget(
            firstProposalName,
            firstProposalLink,
            firstProposalCycles,
            nextSuperBlockHeight,
            firstProposalAddress,
            firstProposalAmountPerCycle)

        # generate 3 blocks to confirm the tx (and update the mnping)
        self.stake(3, [self.remoteOne, self.remoteTwo])

        # activate sporks
        self.activate_spork(self.minerPos, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_13_ENABLE_SUPERBLOCKS")

        txinfo = self.miner.gettransaction(proposalFeeTxId)
        assert_equal(txinfo['amount'], -50.00)

        self.log.info("submitting the budget proposal..")

        proposalHash = self.miner.submitbudget(
            firstProposalName,
            firstProposalLink,
            firstProposalCycles,
            nextSuperBlockHeight,
            firstProposalAddress,
            firstProposalAmountPerCycle,
            proposalFeeTxId)

        # let's wait a little bit and see if all nodes are sync
        time.sleep(1)
        self.check_proposal_existence(firstProposalName, proposalHash)
        self.log.info("proposal broadcast successful!")

        # Proposal is established after 5 minutes. Mine 7 blocks
        # Proposal needs to be on the chain > 5 min.
        self.stake(7, [self.remoteOne, self.remoteTwo])

        # now let's vote for the proposal with the first MN
        self.log.info("Voting with MN1...")
        voteResult = self.ownerOne.mnbudgetvote("alias", proposalHash, "yes", self.masternodeOneAlias, True)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposalName, self.mnOneCollateral.hash, "YES", True)
        self.log.info("all good, MN1 vote accepted everywhere!")

        # now let's vote for the proposal with the second MN
        self.log.info("Voting with MN2...")
        voteResult = self.ownerTwo.mnbudgetvote("alias", proposalHash, "yes", self.masternodeTwoAlias, True)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposalName, self.mnTwoCollateral.hash, "YES", True)
        self.log.info("all good, MN2 vote accepted everywhere!")

        # now let's vote for the proposal with the first DMN
        self.log.info("Voting with DMN1...")
        voteResult = self.ownerOne.mnbudgetvote("alias", proposalHash, "yes", self.proRegTx1)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposalName, self.proRegTx1, "YES", True)
        self.log.info("all good, DMN1 vote accepted everywhere!")

        # Now check the budget
        blockStart = nextSuperBlockHeight
        blockEnd = blockStart + firstProposalCycles * 145
        TotalPayment = firstProposalAmountPerCycle * firstProposalCycles
        Allotted = firstProposalAmountPerCycle
        RemainingPaymentCount = firstProposalCycles
        expected_budget = [
            self.get_proposal_obj(firstProposalName, firstProposalLink, proposalHash, proposalFeeTxId, blockStart,
                                  blockEnd, firstProposalCycles, RemainingPaymentCount, firstProposalAddress, 1,
                                  3, 0, 0, satoshi_round(TotalPayment), satoshi_round(firstProposalAmountPerCycle),
                                  True, True, satoshi_round(Allotted), satoshi_round(Allotted))
                           ]
        self.check_budgetprojection(expected_budget)

        # Quick block count check.
        assert_equal(self.ownerOne.getblockcount(), 276)

        self.log.info("starting budget finalization sync test..")
        self.stake(5, [self.remoteOne, self.remoteTwo])

        # assert that there is no budget finalization first.
        assert_true(len(self.ownerOne.mnfinalbudget("show")) == 0)

        # suggest the budget finalization and confirm the tx (+4 blocks).
        budgetFinHash = self.broadcastbudgetfinalization(self.miner,
                                                         with_ping_mns=[self.remoteOne, self.remoteTwo])
        assert (budgetFinHash != "")
        time.sleep(1)

        self.log.info("checking budget finalization sync..")
        self.check_budget_finalization_sync(0, "OK")

        self.log.info("budget finalization synced!, now voting for the budget finalization..")

        voteResult = self.ownerOne.mnfinalbudget("vote-many", budgetFinHash, True)
        assert_equal(voteResult["detail"][0]["result"], "success")
        self.log.info("Remote One voted successfully.")
        voteResult = self.ownerTwo.mnfinalbudget("vote-many", budgetFinHash, True)
        assert_equal(voteResult["detail"][0]["result"], "success")
        self.log.info("Remote Two voted successfully.")
        voteResult = self.remoteDMN1.mnfinalbudget("vote", budgetFinHash)
        assert_equal(voteResult["detail"][0]["result"], "success")
        self.log.info("DMN voted successfully.")
        self.stake(2, [self.remoteOne, self.remoteTwo])

        self.log.info("checking finalization votes..")
        self.check_budget_finalization_sync(3, "OK")

        self.stake(8, [self.remoteOne, self.remoteTwo])
        addrInfo = self.miner.listreceivedbyaddress(0, False, False, firstProposalAddress)
        assert_equal(addrInfo[0]["amount"], firstProposalAmountPerCycle)

        self.log.info("budget proposal paid!, all good")

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        self.check_budgetprojection(expected_budget)

        self.stake(1, [self.remoteOne, self.remoteTwo])

        # now let's verify that votes expire properly.
        # Drop one MN and one DMN
        self.log.info("expiring MN1..")
        self.spend_collateral(self.ownerOne, self.mnOneCollateral, self.miner)
        self.wait_until_mn_vinspent(self.mnOneCollateral.hash, 30, [self.remoteTwo])
        self.stake(15, [self.remoteTwo]) # create blocks to remove staled votes
        time.sleep(2) # wait a little bit
        self.check_vote_existence(firstProposalName, self.mnOneCollateral.hash, "YES", False)
        self.log.info("MN1 vote expired after collateral spend, all good")

        self.log.info("expiring DMN1..")
        lm = self.ownerOne.listmasternodes(self.proRegTx1)[0]
        self.spend_collateral(self.ownerOne, COutPoint(lm["collateralHash"], lm["collateralIndex"]), self.miner)
        self.wait_until_mn_vinspent(self.proRegTx1, 30, [self.remoteTwo])
        self.stake(15, [self.remoteTwo]) # create blocks to remove staled votes
        time.sleep(2) # wait a little bit
        self.check_vote_existence(firstProposalName, self.proRegTx1, "YES", False)
        self.log.info("DMN vote expired after collateral spend, all good")



if __name__ == '__main__':
    MasternodeGovernanceBasicTest().main()
