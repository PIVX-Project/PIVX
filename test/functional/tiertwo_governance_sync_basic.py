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

import time

from test_framework.messages import COutPoint
from test_framework.test_framework import PivxTier2TestFramework
from test_framework.util import (
    assert_equal,
    assert_true,
    connect_nodes,
    get_datadir_path,
    satoshi_round
)
import shutil
import os

class Proposal:
    def __init__(self, name, link, cycles, payment_addr, amount_per_cycle):
        self.name = name
        self.link = link
        self.cycles = cycles
        self.paymentAddr = payment_addr
        self.amountPerCycle = amount_per_cycle
        self.feeTxId = ""
        self.proposalHash = ""

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
            node.syncwithvalidationinterfacequeue()
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

    def connect_nodes_bi(self, nodes, a, b):
        connect_nodes(nodes[a], b)
        connect_nodes(nodes[b], a)

    def create_proposals_tx(self, props):
        nextSuperBlockHeight = self.miner.getnextsuperblock()
        for entry in props:
            proposalFeeTxId = self.miner.preparebudget(
                entry.name,
                entry.link,
                entry.cycles,
                nextSuperBlockHeight,
                entry.paymentAddr,
                entry.amountPerCycle)
            entry.feeTxId = proposalFeeTxId
        return props

    def propagate_proposals(self, props):
        nextSuperBlockHeight = self.miner.getnextsuperblock()
        for entry in props:
            proposalHash = self.miner.submitbudget(
                entry.name,
                entry.link,
                entry.cycles,
                nextSuperBlockHeight,
                entry.paymentAddr,
                entry.amountPerCycle,
                entry.feeTxId)
            entry.proposalHash = proposalHash
        return props

    def submit_proposals(self, props):
        props = self.create_proposals_tx(props)
        # generate 3 blocks to confirm the tx (and update the mnping)
        self.stake(3, [self.remoteOne, self.remoteTwo])
        # check fee tx existence
        for entry in props:
            txinfo = self.miner.gettransaction(entry.feeTxId)
            assert_equal(txinfo['amount'], -50.00)
        # propagate proposals
        props = self.propagate_proposals(props)
        # let's wait a little bit and see if all nodes are sync
        time.sleep(1)
        for entry in props:
            self.check_proposal_existence(entry.name, entry.proposalHash)
            self.log.info("proposal %s broadcast successful!" % entry.name)
        return props

    def run_test(self):
        self.enable_mocktime()
        self.setup_masternodes_network()
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

        # activate sporks
        self.activate_spork(self.minerPos, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_13_ENABLE_SUPERBLOCKS")
        nextSuperBlockHeight = self.miner.getnextsuperblock()

        # Submit first proposal
        self.log.info("preparing budget proposal..")
        firstProposal = Proposal(
            "super-cool",
            "https://forum.pivx.org/t/test-proposal",
            2,
            self.miner.getnewaddress(),
            300
        )
        self.submit_proposals([firstProposal])

        # Create 15 more proposals to have a higher tier two net gossip movement
        props = []
        for i in range(15):
            props.append(Proposal("prop_"+str(i),
                         "https://link_"+str(i)+".com",
                         3,
                         self.miner.getnewaddress(),
                         11 * (i + 1)))
        self.submit_proposals(props)

        # Proposals are established after 5 minutes. Mine 7 blocks
        # Proposal needs to be on the chain > 5 min.
        self.stake(7, [self.remoteOne, self.remoteTwo])
        # Check proposals existence
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 16)

        # now let's vote for the proposal with the first MN
        self.log.info("Voting with MN1...")
        voteResult = self.ownerOne.mnbudgetvote("alias", firstProposal.proposalHash, "yes", self.masternodeOneAlias, True)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposal.name, self.mnOneCollateral.hash, "YES", True)
        self.log.info("all good, MN1 vote accepted everywhere!")

        # before broadcast the second vote, let's drop the budget data of ownerOne.
        # so the node is forced to send a single proposal sync when the, now orphan, proposal vote is received.
        self.log.info("Testing single proposal re-sync based on an orphan vote, dropping budget data...")
        self.ownerOne.cleanbudget(try_sync=False)
        assert_equal(self.ownerOne.getbudgetprojection(), []) # empty
        assert_equal(self.ownerOne.getbudgetinfo(), [])

        # now let's vote for the proposal with the second MN
        self.log.info("Voting with MN2...")
        voteResult = self.ownerTwo.mnbudgetvote("alias", firstProposal.proposalHash, "yes", self.masternodeTwoAlias, True)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check orphan vote proposal re-sync
        self.log.info("checking orphan vote based proposal re-sync...")
        time.sleep(5) # wait a bit before check it
        self.check_proposal_existence(firstProposal.name, firstProposal.proposalHash)
        self.check_vote_existence(firstProposal.name, self.mnOneCollateral.hash, "YES", True)
        self.log.info("all good, orphan vote based proposal re-sync succeeded")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposal.name, self.mnTwoCollateral.hash, "YES", True)
        self.log.info("all good, MN2 vote accepted everywhere!")

        # now let's vote for the proposal with the first DMN
        self.log.info("Voting with DMN1...")
        voteResult = self.ownerOne.mnbudgetvote("alias", firstProposal.proposalHash, "yes", self.proRegTx1)
        assert_equal(voteResult["detail"][0]["result"], "success")

        # check that the vote was accepted everywhere
        self.stake(1, [self.remoteOne, self.remoteTwo])
        self.check_vote_existence(firstProposal.name, self.proRegTx1, "YES", True)
        self.log.info("all good, DMN1 vote accepted everywhere!")

        # Now check the budget
        blockStart = nextSuperBlockHeight
        blockEnd = blockStart + firstProposal.cycles * 145
        TotalPayment = firstProposal.amountPerCycle * firstProposal.cycles
        Allotted = firstProposal.amountPerCycle
        RemainingPaymentCount = firstProposal.cycles
        expected_budget = [
            self.get_proposal_obj(firstProposal.name, firstProposal.link, firstProposal.proposalHash, firstProposal.feeTxId, blockStart,
                                  blockEnd, firstProposal.cycles, RemainingPaymentCount, firstProposal.paymentAddr, 1,
                                  3, 0, 0, satoshi_round(TotalPayment), satoshi_round(firstProposal.amountPerCycle),
                                  True, True, satoshi_round(Allotted), satoshi_round(Allotted))
                           ]
        self.check_budgetprojection(expected_budget)

        # Quick block count check.
        assert_equal(self.ownerOne.getblockcount(), 279)

        self.log.info("starting budget finalization sync test..")
        self.stake(2, [self.remoteOne, self.remoteTwo])

        # assert that there is no budget finalization first.
        assert_equal(len(self.ownerOne.mnfinalbudget("show")), 0)

        # suggest the budget finalization and confirm the tx (+4 blocks).
        budgetFinHash = self.broadcastbudgetfinalization(self.miner,
                                                         with_ping_mns=[self.remoteOne, self.remoteTwo])
        assert (budgetFinHash != "")
        time.sleep(2)

        self.log.info("checking budget finalization sync..")
        self.check_budget_finalization_sync(0, "OK")

        self.log.info("budget finalization synced!, now voting for the budget finalization..")
        # Connecting owner to all the other nodes.
        self.connect_to_all(self.ownerOnePos)

        voteResult = self.ownerOne.mnfinalbudget("vote-many", budgetFinHash, True)
        assert_equal(voteResult["detail"][0]["result"], "success")
        time.sleep(2) # wait a bit
        self.stake(2, [self.remoteOne, self.remoteTwo])
        self.check_budget_finalization_sync(1, "OK")
        self.log.info("Remote One voted successfully.")

        # before broadcast the second finalization vote, let's drop the budget data of remoteOne.
        # so the node is forced to send a single fin sync when the, now orphan, vote is received.
        self.log.info("Testing single fin re-sync based on an orphan vote, dropping budget data...")
        self.remoteOne.cleanbudget(try_sync=False)
        assert_equal(self.remoteOne.getbudgetprojection(), []) # empty
        assert_equal(self.remoteOne.getbudgetinfo(), [])

        # vote for finalization with MN2 and the DMN
        voteResult = self.ownerTwo.mnfinalbudget("vote-many", budgetFinHash, True)
        assert_equal(voteResult["detail"][0]["result"], "success")
        self.log.info("Remote Two voted successfully.")
        voteResult = self.remoteDMN1.mnfinalbudget("vote", budgetFinHash)
        assert_equal(voteResult["detail"][0]["result"], "success")
        self.log.info("DMN voted successfully.")
        time.sleep(2) # wait a bit
        self.stake(2, [self.remoteOne, self.remoteTwo])

        self.log.info("checking finalization votes..")
        self.check_budget_finalization_sync(3, "OK")
        self.log.info("orphan vote based finalization re-sync succeeded")

        self.stake(6, [self.remoteOne, self.remoteTwo])
        addrInfo = self.miner.listreceivedbyaddress(0, False, False, firstProposal.paymentAddr)
        assert_equal(addrInfo[0]["amount"], firstProposal.amountPerCycle)

        self.log.info("budget proposal paid!, all good")

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        self.check_budgetprojection(expected_budget)

        self.stake(1, [self.remoteOne, self.remoteTwo])

        self.log.info("checking resync (1): cleaning budget data only..")
        # now let's drop budget data and try to re-sync it.
        self.remoteOne.cleanbudget(True)
        assert_equal(self.remoteOne.mnsync("status")["RequestedMasternodeAssets"], 0)
        assert_equal(self.remoteOne.getbudgetprojection(), []) # empty
        assert_equal(self.remoteOne.getbudgetinfo(), [])

        self.log.info("budget cleaned, starting resync")
        self.wait_until_mnsync_finished()
        self.check_budgetprojection(expected_budget)
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 16)

        self.log.info("resync (1): budget data resynchronized successfully!")

        self.log.info("checking resync (2): stop node, delete chain data and resync from scratch..")
        # stop and remove everything
        self.stop_node(self.ownerTwoPos)
        ownerTwoDir = os.path.join(get_datadir_path(self.options.tmpdir, self.ownerTwoPos), "regtest")
        for entry in ['chainstate', 'blocks', 'sporks', 'evodb', 'zerocoin', "mncache.dat", "budget.dat", "mnpayments.dat", "peers.dat"]:
            rem_path = os.path.join(ownerTwoDir, entry)
            shutil.rmtree(rem_path) if os.path.isdir(rem_path) else os.remove(rem_path)

        self.log.info("restarting node..")
        self.start_node(self.ownerTwoPos)
        self.ownerTwo.setmocktime(self.mocktime)
        self.connect_to_all(self.ownerTwoPos)
        self.stake(2, [self.remoteOne, self.remoteTwo])

        self.log.info("syncing node..")
        self.wait_until_mnsync_finished()
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 16)
        self.log.info("resync (2): budget data resynchronized successfully!")

        # Let's now verify the remote budget data relay.
        # Drop the budget data and generate blocks until someone incrementally sync us
        # (this is done once every 28 blocks on regtest).
        self.log.info("Testing incremental sync from peers, dropping budget data...")
        self.remoteDMN1.cleanbudget(try_sync=False)
        assert_equal(self.remoteDMN1.getbudgetprojection(), []) # empty
        assert_equal(self.remoteDMN1.getbudgetinfo(), [])
        self.log.info("Generating blocks until someone syncs the node..")
        self.stake(40, [self.remoteOne, self.remoteTwo])
        time.sleep(5) # wait a little bit
        self.log.info("Checking budget sync..")
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 16)
        self.check_vote_existence(firstProposal.name, self.mnOneCollateral.hash, "YES", True)
        self.check_vote_existence(firstProposal.name, self.mnTwoCollateral.hash, "YES", True)
        self.check_vote_existence(firstProposal.name, self.proRegTx1, "YES", True)
        self.check_budget_finalization_sync(3, "OK")
        self.log.info("Remote incremental sync succeeded")

        # now let's verify that votes expire properly.
        # Drop one MN and one DMN
        self.log.info("expiring MN1..")
        self.spend_collateral(self.ownerOne, self.mnOneCollateral, self.miner)
        self.wait_until_mn_vinspent(self.mnOneCollateral.hash, 30, [self.remoteTwo])
        self.stake(15, [self.remoteTwo]) # create blocks to remove staled votes
        time.sleep(2) # wait a little bit
        self.check_vote_existence(firstProposal.name, self.mnOneCollateral.hash, "YES", False)
        self.check_budget_finalization_sync(2, "OK") # budget finalization vote removal
        self.log.info("MN1 vote expired after collateral spend, all good")

        self.log.info("expiring DMN1..")
        lm = self.ownerOne.listmasternodes(self.proRegTx1)[0]
        self.spend_collateral(self.ownerOne, COutPoint(lm["collateralHash"], lm["collateralIndex"]), self.miner)
        self.wait_until_mn_vinspent(self.proRegTx1, 30, [self.remoteTwo])
        self.stake(15, [self.remoteTwo]) # create blocks to remove staled votes
        time.sleep(2) # wait a little bit
        self.check_vote_existence(firstProposal.name, self.proRegTx1, "YES", False)
        self.check_budget_finalization_sync(1, "OK") # budget finalization vote removal
        self.log.info("DMN vote expired after collateral spend, all good")

        # Check that the budget is removed 200 blocks after the last payment
        assert_equal(len(self.miner.mnfinalbudget("show")), 1)
        blocks_to_mine = nextSuperBlockHeight + 200 - self.miner.getblockcount()
        self.log.info("Mining %d more blocks to check expired budget removal..." % blocks_to_mine)
        self.stake(blocks_to_mine - 1, [self.remoteTwo])
        # finalized budget must still be there
        self.miner.checkbudgets()
        assert_equal(len(self.miner.mnfinalbudget("show")), 1)
        # after one more block it must be removed
        self.stake(1, [self.remoteTwo])
        self.miner.checkbudgets()
        assert_equal(len(self.miner.mnfinalbudget("show")), 0)
        self.log.info("All good.")

if __name__ == '__main__':
    MasternodeGovernanceBasicTest().main()
