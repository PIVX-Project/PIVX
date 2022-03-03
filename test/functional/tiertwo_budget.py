#!/usr/bin/env python3
# Copyright (c) 2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking:
 1) pre-v6 multi-block payments
 2) different payment ordering validation (proposals paid in different order)
 3) duplicated payments validation --> wrong path
 4) post-v6 single block payments
 5) single block payments ordering validation (proposals paid in different order)
 6) duplicated payments validation --> wrong path
"""

import time

from decimal import Decimal
from test_framework.test_framework import PivxTier2TestFramework
from test_framework.util import (
    assert_equal
)
from test_framework.budget_util import (
    check_budget_finalization_sync,
    create_proposals_tx,
    check_budgetprojection,
    check_proposal_existence,
    check_mn_list,
    check_mns_status_legacy,
    check_vote_existence,
    get_proposal,
    Proposal,
    propagate_proposals
)
from test_framework.messages import (
    COIN
)

class BudgetTest(PivxTier2TestFramework):

    def set_test_params(self):
        self.v6_enforcement_height = 300
        super().set_test_params()

    def broadcastbudgetfinalization(self, node, with_ping_mns=[]):
        self.log.info("suggesting the budget finalization..")
        assert (node.mnfinalbudgetsuggest() is not None)

        self.log.info("confirming the budget finalization..")
        time.sleep(1)
        self.stake(4, with_ping_mns)

        self.log.info("broadcasting the budget finalization..")
        return node.mnfinalbudgetsuggest()

    def submit_proposals(self, props):
        props = create_proposals_tx(self.miner, props)
        # generate 3 blocks to confirm the tx (and update the mnping)
        self.stake(3, [self.remoteOne, self.remoteTwo, self.remoteThree])
        # check fee tx existence
        for entry in props:
            txinfo = self.miner.gettransaction(entry.feeTxId)
            assert_equal(txinfo['amount'], -50.00)
        # propagate proposals
        props = propagate_proposals(self.miner, props)
        # let's wait a little bit and see if all nodes are sync
        time.sleep(1)
        for entry in props:
            check_proposal_existence(self.nodes, entry.name, entry.proposalHash)
            self.log.info("proposal %s broadcast successful!" % entry.name)
        return props

    def vote_legacy(self, node_voter, proposal, vote_direction, mn_voter_alias):
        self.log.info("Voting with " + mn_voter_alias + ", for: " + proposal.name)
        voteResult = node_voter.mnbudgetvote("alias", proposal.proposalHash, vote_direction, mn_voter_alias, True)
        assert_equal(voteResult["detail"][0]["result"], "success")
        time.sleep(1)

    def vote(self, node_voter, proposal, vote_direction, pro_reg_tx):
        self.log.info("Voting with DMN " + pro_reg_tx + ", for: " + proposal.name)
        voteResult = node_voter.mnbudgetvote("alias", proposal.proposalHash, vote_direction, pro_reg_tx)
        assert_equal(voteResult["detail"][0]["result"], "success")
        time.sleep(1)

    def vote_finalization(self, voting_node, budget_fin_hash, legacy):
        voteResult = voting_node.mnfinalbudget("vote-many" if legacy else "vote", budget_fin_hash, legacy)
        assert_equal(voteResult["detail"][0]["result"], "success")

    def check_address_balance(self, addr, expected_balance, has_balance=True):
        addrInfo = self.nodes[self.ownerOnePos].listreceivedbyaddress(0, False, False, addr)
        if has_balance:
            assert_equal(addrInfo[0]["amount"], expected_balance)
        else:
            assert_equal(len(addrInfo), 0)

    def check_block_proposal_payment(self, block_hash, expected_to_address, expected_to_value, expected_out_index, is_v6_active):
        block = self.miner.getblock(block_hash)
        if is_v6_active:
            # Get the coinbase second output that is the proposal payment
            coinbase_tx = self.miner.getrawtransaction(block["tx"][0], True)
            proposal_out = coinbase_tx["vout"][expected_out_index]
            assert_equal(proposal_out["value"], expected_to_value)
            assert_equal(proposal_out["scriptPubKey"]["addresses"][0], expected_to_address)
        else:
            # Get the coinstake third output
            coinstake_tx = self.miner.getrawtransaction(block["tx"][1], True)
            proposal_out = coinstake_tx["vout"][expected_out_index]
            assert_equal(proposal_out["value"], expected_to_value)
            assert_equal(proposal_out["scriptPubKey"]["addresses"][0], expected_to_address)

    def finalize_and_vote_budget(self):
        # suggest the budget finalization and confirm the tx (+4 blocks).
        budgetFinHash = self.broadcastbudgetfinalization(self.miner,
                                                         with_ping_mns=[self.remoteOne, self.remoteTwo, self.remoteThree])
        assert (budgetFinHash != "")
        time.sleep(2)
        print(budgetFinHash)
        self.log.info("voting budget finalization..")
        for node in [self.ownerOne, self.ownerTwo, self.ownerThree]:
            self.vote_finalization(node, budgetFinHash, True)
        time.sleep(2) # wait a bit
        check_budget_finalization_sync(self.nodes, 3, "OK")

    def run_test(self):
        self.enable_mocktime()
        self.setup_masternodes_network(setup_dmn=False)
        txHashSet = set([self.mnOneCollateral.hash, self.mnTwoCollateral.hash, self.mnThreeCollateral.hash])
        # check mn list from miner
        check_mn_list(self.miner, txHashSet)

        # check status of masternodes
        check_mns_status_legacy(self.remoteOne, self.mnOneCollateral.hash)
        check_mns_status_legacy(self.remoteTwo, self.mnTwoCollateral.hash)
        check_mns_status_legacy(self.remoteThree, self.mnThreeCollateral.hash)

        # activate sporks
        self.activate_spork(self.minerPos, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerPos, "SPORK_13_ENABLE_SUPERBLOCKS")
        nextSuperBlockHeight = self.miner.getnextsuperblock()

        # Submit first proposal
        self.log.info("preparing budget proposal..")
        # Create 15 more proposals to have a higher tier two net gossip movement
        props = []
        for i in range(16):
            props.append(Proposal("prop_"+str(i),
                         "https://link_"+str(i)+".com",
                         4,
                         self.nodes[self.ownerOnePos].getnewaddress(),
                         11 * (i + 1)))
        self.submit_proposals(props)

        # Proposals are established after 5 minutes. Mine 7 blocks
        # Proposal needs to be on the chain > 5 min.
        self.stake(7, [self.remoteOne, self.remoteTwo, self.remoteThree])
        # Check proposals existence
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 16)

        # now let's vote for the two first proposals
        expected_budget = []
        blockStart = nextSuperBlockHeight
        alloted = 0
        for i in range(2):
            prop = props[i]
            self.vote_legacy(self.ownerOne, prop, "yes", self.masternodeOneAlias)
            check_vote_existence(self.nodes, prop.name, self.mnOneCollateral.hash, "YES", True)
            self.vote_legacy(self.ownerTwo, prop, "yes", self.masternodeTwoAlias)
            check_vote_existence(self.nodes, prop.name, self.mnTwoCollateral.hash, "YES", True)
            if i < 1:
                self.vote_legacy(self.ownerThree, prop, "yes", self.masternodeThreeAlias)
                check_vote_existence(self.nodes, prop.name, self.mnThreeCollateral.hash, "YES", True)
            alloted += prop.amountPerCycle
            expected_budget.append(get_proposal(prop, blockStart, prop.amountPerCycle, alloted, 3 - i))

        # Now check the budget
        check_budgetprojection(self.nodes, expected_budget, self.log)

        # Quick block count check.
        assert_equal(self.ownerOne.getblockcount(), 272)
        self.stake(10, [self.remoteOne, self.remoteTwo, self.remoteThree])
        # Finalize budget
        self.finalize_and_vote_budget()
        self.stake(2, [self.remoteOne, self.remoteTwo, self.remoteThree])

        # Check first proposal payment
        prop1 = props[0]
        self.check_block_proposal_payment(self.miner.getbestblockhash(), prop1.paymentAddr, prop1.amountPerCycle, 2, False)
        self.check_address_balance(prop1.paymentAddr, prop1.amountPerCycle)

        # Check second proposal payment
        prop2 = props[1]
        assert prop2.paymentAddr is not prop1.paymentAddr
        self.check_address_balance(prop2.paymentAddr, 0, has_balance=False)
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])
        self.check_block_proposal_payment(self.miner.getbestblockhash(), prop2.paymentAddr, prop2.amountPerCycle, 2, False)
        self.check_address_balance(prop2.paymentAddr, prop2.amountPerCycle)

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        expected_budget[1]["RemainingPaymentCount"] -= 1
        check_budgetprojection(self.nodes, expected_budget, self.log)
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])

        self.log.info("pre-v6 budget proposal paid, all good. Testing enforcement now..")

        ##################################################################
        self.log.info("checking post enforcement coinstake value..")
        # Now test post enforcement, active from block 300
        for _ in range(4):
            self.miner.generate(30)
            self.stake_and_ping(self.minerPos, 1, [self.remoteOne, self.remoteTwo, self.remoteThree])

        # Post-v6 enforcement
        # Get the coinstake and check that the input value is equal to
        # the output value + block reward - MN payment.
        BLOCK_REWARD = Decimal(250 * COIN)
        MN_BLOCK_REWARD = Decimal(3 * COIN)
        tx_coinstake_id = self.miner.getblock(self.miner.getbestblockhash(), True)["tx"][1]
        tx_coinstake = self.miner.getrawtransaction(tx_coinstake_id, True)
        tx_coinstake_out_value = Decimal(tx_coinstake["vout"][1]["value"]) * COIN
        tx_coinstake_vin = tx_coinstake["vin"][0]
        tx_coinstake_input = self.miner.getrawtransaction(tx_coinstake_vin["txid"], True)
        tx_coinstake_input_value = Decimal(tx_coinstake_input["vout"][int(tx_coinstake_vin["vout"])]["value"]) * COIN
        assert(tx_coinstake_out_value == tx_coinstake_input_value + BLOCK_REWARD - MN_BLOCK_REWARD)

        ##############################################################
        # Check single block payments
        self.log.info("mining until next superblock..")
        next_super_block = self.miner.getnextsuperblock()
        block_count = self.miner.getblockcount()
        self.stake_and_ping(self.minerPos, next_super_block - block_count - 6, [self.remoteOne, self.remoteTwo, self.remoteThree])
        assert_equal(self.ownerOne.getblockcount(), 426)
        # Finalize budget
        self.finalize_and_vote_budget()
        self.stake(2, [self.remoteOne, self.remoteTwo, self.remoteThree])
        self.log.info("checking single block payments..")
        assert_equal(self.ownerOne.getblockcount(), 432)
        self.check_block_proposal_payment(self.miner.getbestblockhash(), prop1.paymentAddr, prop1.amountPerCycle, 1, True)
        self.check_block_proposal_payment(self.miner.getbestblockhash(), prop2.paymentAddr, prop2.amountPerCycle, 2, True)

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        expected_budget[1]["RemainingPaymentCount"] -= 1
        check_budgetprojection(self.nodes, expected_budget, self.log)
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])
        self.log.info("post-v6 budget proposal paid, all good.")

        ##################################################################
        self.log.info("Now test proposal with duplicate script and value")

        self.proRegTx1, self.dmn1Privkey = self.setupDMN(
            self.ownerOne,
            self.miner,
            self.remoteDMN1Pos,
            "fund"
        )
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])
        time.sleep(3)
        self.advance_mocktime(10)
        self.remoteDMN1.initmasternode(self.dmn1Privkey)
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])

        # Now test creating a new proposal paying to the same script and value as prop1
        blockStart = self.miner.getnextsuperblock()
        prop17 = Proposal("prop_17", "https://link_"+str(17)+".com", 2, prop1.paymentAddr, 11)
        self.submit_proposals([prop17])
        self.stake(5, [self.remoteOne, self.remoteTwo, self.remoteThree])
        for i in range(self.num_nodes):
            assert_equal(len(self.nodes[i].getbudgetinfo()), 17)
        # vote prop17
        self.vote_legacy(self.ownerOne, prop17, "yes", self.masternodeOneAlias)
        check_vote_existence(self.nodes, prop17.name, self.mnOneCollateral.hash, "YES", True)
        self.vote_legacy(self.ownerTwo, prop17, "yes", self.masternodeTwoAlias)
        check_vote_existence(self.nodes, prop17.name, self.mnTwoCollateral.hash, "YES", True)
        self.vote_legacy(self.ownerThree, prop17, "yes", self.masternodeThreeAlias)
        check_vote_existence(self.nodes, prop17.name, self.mnThreeCollateral.hash, "YES", True)
        self.vote(self.ownerOne, prop17, "yes", self.proRegTx1)
        check_vote_existence(self.nodes, prop17.name, self.proRegTx1, "YES", True)

        alloted += prop17.amountPerCycle
        expected_budget.insert(0, get_proposal(prop17, blockStart, prop17.amountPerCycle, prop17.amountPerCycle, 4))
        expected_budget[1]["TotalBudgetAllotted"] = Decimal('22.00000000')
        expected_budget[2]["TotalBudgetAllotted"] = Decimal('44.00000000')
        # Now check the budget
        check_budgetprojection(self.nodes, expected_budget, self.log)

        for _ in range(4):
            self.miner.generate(30)
            self.stake_and_ping(self.minerPos, 1, [self.remoteOne, self.remoteTwo, self.remoteThree])
        next_super_block = self.miner.getnextsuperblock()
        block_count = self.miner.getblockcount()
        self.stake_and_ping(self.minerPos, next_super_block - block_count - 6, [self.remoteOne, self.remoteTwo, self.remoteThree])
        assert_equal(self.ownerOne.getblockcount(), 570)

        # Finalize budget
        self.finalize_and_vote_budget()
        self.stake(2, [self.remoteOne, self.remoteTwo, self.remoteThree])
        self.check_address_balance(prop1.paymentAddr, prop1.amountPerCycle * 4) # prop1.amountPerCycle * 3 + prop17.amountPerCycle
        self.check_address_balance(prop2.paymentAddr, prop2.amountPerCycle * 3)

        # Check that the proposal info returns updated payment count
        expected_budget[0]["RemainingPaymentCount"] -= 1
        expected_budget[1]["RemainingPaymentCount"] -= 1
        expected_budget[2]["RemainingPaymentCount"] -= 1
        check_budgetprojection(self.nodes, expected_budget, self.log)
        self.stake(1, [self.remoteOne, self.remoteTwo, self.remoteThree])
        self.log.info("post-v6 duplicate proposals payouts paid.")


if __name__ == '__main__':
    BudgetTest().main()
