#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Exercise the wallet.  Ported from wallet.sh.
# Does the following:
#   a) creates 3 nodes, with an empty chain (no blocks).
#   b) node0 mines a block
#   c) node1 mines 32 blocks, so now node 0 has 60001phr, node 1 has 4250phr, node2 has none.
#   d) node0 sends 601 phr to node2, in two transactions (301 phr, then 300 phr).
#   e) node0 mines a block, collects the fee on the second transaction
#   f) node1 mines 16 blocks, to mature node0's just-mined block
#   g) check that node0 has 100-21, node2 has 21
#   h) node0 should now have 2 unspent outputs;  send these to node2 via raw tx broadcast by node1
#   i) have node1 mine a block
#   j) check balances - node0 should have 0, node2 should have 100
#

from test_framework.util import *
from test_case_base import TestCaseBase

class WalletTest(TestCaseBase):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def test_wallet_tutorial(self):
        print("Mining blocks...")

        self.nodes[0].setgenerate(True, 1)

        self.sync_all()
        self.nodes[1].setgenerate(True, 32)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 17500000)
        assert_equal(self.nodes[1].getbalance(), 80000)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Send 601 BTC from 0 to 2 using sendtoaddress call.
        # Second transaction will be child of first, and will require a fee
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 351)
        self.nodes[0].setgenerate(True, 1)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 350)

        # Have node0 mine a block, thus he will collect his own fee.
        self.nodes[0].setgenerate(True, 1)
        self.sync_all()

        # Have node1 generate 100 blocks (so node0 can recover the fee)
        self.nodes[1].setgenerate(True, 16)
        self.sync_all()

        # node0 should end up with 100 btc in block rewards plus fees, but
        # minus the 21 plus fees sent to node2
        assert(self.nodes[0].getbalance() > 17504298)
        assert_equal(self.nodes[2].getbalance(), 701)

        # Node0 should have two unspent outputs.
        # Create a couple of transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), 3)

        # create 3 transactions
        txns_to_send = []
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
            outputs[self.nodes[2].getnewaddress("from1")] = utxo["amount"]
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransaction(raw_tx))

        # Have node 1 (miner) send the transactions
        self.nodes[1].sendrawtransaction(txns_to_send[0]["hex"], True)
        self.nodes[1].sendrawtransaction(txns_to_send[1]["hex"], True)
        tx = self.nodes[1].sendrawtransaction(txns_to_send[2]["hex"], True)

        # Have node1 mine a block to confirm transactions:
        self.nodes[1].setgenerate(True, 1)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 0)

        # Test basic functionality of dumpprivkey
        self.nodes[0].dumpprivkey(self.nodes[0].getnewaddress())


if __name__ == '__main__':
    WalletTest().main()
