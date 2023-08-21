#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import HemisTestFramework
from test_framework.util import assert_equal, connect_nodes, get_coinstake_address

class WalletAnchorForkTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        saplingUpgrade = ['-nuparams=v5_shield:1']
        self.extra_args = [saplingUpgrade, saplingUpgrade, saplingUpgrade]

    def run_test (self):
        self.nodes[0].generate(4) # generate blocks to activate sapling in regtest
        # verify sapling activation.
        assert(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['activationheight'] == 1)

        self.sync_all()

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(self.nodes[0].getblockcount(), walletinfo['last_processed_block'])
        assert_equal(walletinfo['immature_balance'], 1000)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all()
        self.nodes[1].generate(102)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 1000)
        assert_equal(self.nodes[1].getbalance(), 500)
        assert_equal(self.nodes[2].getbalance(), 0)

        # At this point in time, commitment tree is the empty root

        # Node 0 creates a shield transaction
        mytaddr0 = get_coinstake_address(self.nodes[0])
        myzaddr0 = self.nodes[0].getnewshieldaddress()
        recipients = []
        recipients.append({"address":myzaddr0, "amount": Decimal('10.0') - Decimal('0.0001')})
        txid = self.nodes[0].shieldsendmany(mytaddr0, recipients)

        # Sync up mempools and mine the transaction.  All nodes have the same anchor.
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Stop nodes.
        self.stop_nodes()

        # Relaunch nodes and partition network into two:
        # A: node 0
        # B: node 1, 2

        self.start_node(0, self.extra_args[0])
        self.start_node(1, self.extra_args[1])
        self.start_node(2, self.extra_args[2])
        connect_nodes(self.nodes[1], 2)

        # Partition B, node 1 mines an empty block
        self.nodes[1].generate(1)
        self.sync_blocks(self.nodes[1:3])

        # Check partition
        assert_equal(self.nodes[1].getblockcount(), self.nodes[2].getblockcount())
        assert(self.nodes[2].getblockcount() != self.nodes[0].getblockcount())

        # Partition A, node 0 creates a shield transaction
        recipients = []
        recipients.append({"address":myzaddr0, "amount": Decimal('10.0') - Decimal('0.0001')})
        txid = self.nodes[0].shieldsendmany(mytaddr0, recipients)
        rawhex = self.nodes[0].getrawtransaction(txid)

        # Partition A, node 0 mines a block with the transaction
        self.nodes[0].generate(1)
        self.sync_all(self.nodes[1:3])

        # Partition B, node 1 mines the same shield transaction
        txid2 = self.nodes[1].sendrawtransaction(rawhex)
        assert_equal(txid, txid2)
        self.nodes[1].generate(1)

        # Check that Partition B is one block ahead and that they have different tips
        assert_equal(self.nodes[0].getblockcount() + 1, self.nodes[1].getblockcount())
        assert(self.nodes[0].getbestblockhash() != self.nodes[1].getbestblockhash())

        # Shut down all nodes so any in-memory state is saved to disk
        self.stop_nodes()

        # Relaunch nodes and reconnect the entire network
        self.start_nodes()
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[2], 1)
        connect_nodes(self.nodes[2], 0)

        # Mine a new block and let it propagate
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal( self.nodes[0].getbestblockhash(), self.nodes[1].getbestblockhash())
        assert_equal( self.nodes[1].getbestblockhash(), self.nodes[2].getbestblockhash())

if __name__ == '__main__':
    WalletAnchorForkTest().main()
