#!/usr/bin/env python3
# Copyright (c) 2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
import os
import time

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    p2p_port,
    set_node_times,
)


class GovernanceReorgTest(HemisTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        # 4 nodes:
        # - 1 miner/mncontroller
        # - 2 remote mns
        # - 1 other node to stake a forked chain
        self.num_nodes = 4
        self.extra_args = [["-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi"],
                           [],
                           ["-listen", "-externalip=127.0.0.1"],
                           ["-listen", "-externalip=127.0.0.1"],
                           ]
        self.enable_mocktime()

        self.minerAPos = 0
        self.minerBPos = 1
        self.remoteOnePos = 1
        self.remoteTwoPos = 2

        self.masternodeOneAlias = "mnOne"
        self.masternodeTwoAlias = "mntwo"

        self.mnOnePrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"
        self.mnTwoPrivkey = "92Hkebp3RHdDidGZ7ARgS4orxJAGyFUPDXNqtsYsiwho1HGVRbF"

    def run_test(self):
        minerA = self.nodes[self.minerAPos]     # also controller of mn1 and mn2
        minerB = self.nodes[self.minerBPos]
        mn1 = self.nodes[self.remoteOnePos]
        mn2 = self.nodes[self.remoteTwoPos]

        # First mine 250 PoW blocks (50 with minerB, 200 with minerA)
        self.log.info("Generating 259 blocks...")
        for _ in range(2):
            for _ in range(25):
                self.mocktime = self.generate_pow(self.minerBPos, self.mocktime)
            self.sync_blocks()
            for _ in range(100):
                self.mocktime = self.generate_pow(self.minerAPos, self.mocktime)
            self.sync_blocks()
        # Then stake 9 blocks with minerA
        self.stake_and_ping(self.minerAPos, 9, [])
        for n in self.nodes:
            assert_equal(n.getblockcount(), 259)

        # Setup Masternodes
        self.log.info("Masternodes setup...")
        ownerdir = os.path.join(self.options.tmpdir, "node%d" % self.minerAPos, "regtest")
        self.mnOneCollateral = self.setupMasternode(minerA, minerA, self.masternodeOneAlias,
                                                    ownerdir, self.remoteOnePos, self.mnOnePrivkey)
        self.mnTwoCollateral = self.setupMasternode(minerA, minerA, self.masternodeTwoAlias,
                                                    ownerdir, self.remoteTwoPos, self.mnTwoPrivkey)

        # Activate masternodes
        self.log.info("Masternodes activation...")
        self.stake_and_ping(self.minerAPos, 1, [])
        time.sleep(3)
        self.advance_mocktime(10)
        remoteOnePort = p2p_port(self.remoteOnePos)
        remoteTwoPort = p2p_port(self.remoteTwoPos)
        mn1.initmasternode(self.mnOnePrivkey, "127.0.0.1:"+str(remoteOnePort))
        mn2.initmasternode(self.mnTwoPrivkey, "127.0.0.1:"+str(remoteTwoPort))
        self.stake_and_ping(self.minerAPos, 1, [])
        self.wait_until_mnsync_finished()
        self.controller_start_masternodes(minerA, [self.masternodeOneAlias, self.masternodeTwoAlias])
        self.wait_until_mn_preenabled(self.mnOneCollateral.hash, 40)
        self.wait_until_mn_preenabled(self.mnOneCollateral.hash, 40)
        self.send_3_pings([mn1, mn2])
        self.wait_until_mn_enabled(self.mnOneCollateral.hash, 120, [mn1, mn2])
        self.wait_until_mn_enabled(self.mnOneCollateral.hash, 120, [mn1, mn2])

        # activate sporks
        self.log.info("Masternodes enabled. Activating sporks.")
        self.activate_spork(self.minerAPos, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        self.activate_spork(self.minerAPos, "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT")
        self.activate_spork(self.minerAPos, "SPORK_13_ENABLE_SUPERBLOCKS")

        # Create a proposal and vote on it
        next_superblock = minerA.getnextsuperblock()
        payee = minerA.getnewaddress()
        self.log.info("Creating a proposal to be paid at block %d" % next_superblock)
        proposalFeeTxId = minerA.preparebudget("test1", "https://test1.org", 2,
                                               next_superblock, payee, 300)
        self.stake_and_ping(self.minerAPos, 3, [mn1, mn2])
        proposalHash = minerA.submitbudget("test1", "https://test1.org", 2,
                                           next_superblock, payee, 300, proposalFeeTxId)
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 7, [mn1, mn2])
        self.log.info("Vote for the proposal and check projection...")
        minerA.mnbudgetvote("alias", proposalHash, "yes", self.masternodeOneAlias)
        minerA.mnbudgetvote("alias", proposalHash, "yes", self.masternodeTwoAlias)
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 1, [mn1, mn2])
        projection = minerB.getbudgetprojection()[0]
        assert_equal(projection["Name"], "test1")
        assert_equal(projection["Hash"], proposalHash)
        assert_equal(projection["Yeas"], 2)

        # Create the finalized budget and vote on it
        self.log.info("Finalizing the budget...")
        self.stake_and_ping(self.minerAPos, 5, [mn1, mn2])
        assert (minerA.mnfinalbudgetsuggest() is not None)
        time.sleep(1)
        self.stake_and_ping(self.minerAPos, 4, [mn1, mn2])
        budgetFinHash = minerA.mnfinalbudgetsuggest()
        assert (budgetFinHash != "")
        time.sleep(1)
        minerA.mnfinalbudget("vote-many", budgetFinHash)
        self.stake_and_ping(self.minerAPos, 2, [mn1, mn2])
        budFin = minerB.mnfinalbudget("show")
        budget = budFin[next(iter(budFin))]
        assert_equal(budget["VoteCount"], 2)

        # Stake up until the block before the superblock.
        skip_blocks = next_superblock - minerA.getblockcount() - 1
        self.stake_and_ping(self.minerAPos, skip_blocks, [mn1, mn2])

        # Split the network.
        self.log.info("Splitting the chain at block %d" % minerA.getblockcount())
        self.split_network()

        # --- Chain A ---
        self.nodes.pop(self.minerBPos)
        # mine the superblock and check payment
        self.log.info("Checking superblock on chain A...")
        self.create_and_check_superblock(minerA, next_superblock, payee)
        # Add 10 blocks on top
        self.log.info("Staking 10 blocks...")
        self.stake_and_ping(self.nodes.index(minerA), 10, [mn1, mn2])

        # --- Chain B ---
        other_nodes = self.nodes.copy()
        self.nodes = [minerB]
        # mine the superblock and check payment
        self.log.info("Checking superblock on chain B...")
        self.create_and_check_superblock(minerB, next_superblock, payee)
        # Add 1 single block on top
        self.log.info("Staking 1 block...")
        self.stake_and_ping(self.nodes.index(minerB), 1, [])

        # --- Reconnect nodes --
        self.log.info("Reconnecting and re-organizing blocks...")
        self.nodes = other_nodes
        self.nodes.insert(self.minerBPos, minerB)
        set_node_times(self.nodes, self.mocktime)
        self.reconnect_nodes()
        self.sync_all()
        assert_equal(minerB.getblockcount(), next_superblock + 10)
        assert_equal(minerB.getbestblockhash(), minerA.getbestblockhash())

        self.log.info("All good.")


    def send_3_pings(self, mn_list):
        self.advance_mocktime(30)
        self.send_pings(mn_list)
        self.stake_and_ping(self.minerAPos, 1, mn_list)
        self.advance_mocktime(30)
        self.send_pings(mn_list)
        time.sleep(2)

    def split_network(self):
        for i in range(self.num_nodes):
            if i != self.minerBPos:
                disconnect_nodes(self.nodes[i], self.minerBPos)
                disconnect_nodes(self.nodes[self.minerBPos], i)
        # by-pass ring connection
        assert self.minerBPos > 0
        connect_nodes(self.nodes[self.minerBPos-1], self.minerBPos+1)

    def reconnect_nodes(self):
        for i in range(self.num_nodes):
            if i != self.minerBPos:
                connect_nodes(self.nodes[i], self.minerBPos)
                connect_nodes(self.nodes[self.minerBPos], i)

    def create_and_check_superblock(self, node, next_superblock, payee):
        self.stake_and_ping(self.nodes.index(node), 1, [])
        assert_equal(node.getblockcount(), next_superblock)
        coinstake = node.getrawtransaction(node.getblock(node.getbestblockhash())["tx"][1], True)
        budget_payment_out = coinstake["vout"][-1]
        assert_equal(budget_payment_out["value"], Decimal("300"))
        assert_equal(budget_payment_out["scriptPubKey"]["addresses"][0], payee)


if __name__ == '__main__':
    GovernanceReorgTest().main()
