#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listtransactions API."""

from decimal import Decimal
from io import BytesIO

from test_framework.messages import CTransaction
from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_array_result,
    assert_equal,
    hex_str_to_bytes,
)

def txFromHex(hexstring):
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(hexstring))
    tx.deserialize(f)
    return tx

class ListTransactionsTest(HemisTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        # whitelist all peers to speed up tx relay / mempool sync
        self.extra_args = [["-whitelist=127.0.0.1"]] * self.num_nodes
        self.enable_mocktime()

    def run_test(self):
        # Simple send, 0 to 1:
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid},
                           {"category": "send", "amount": Decimal("-0.1"), "confirmations": 0})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"txid": txid},
                           {"category": "receive", "amount": Decimal("0.1"), "confirmations": 0})
        # mine a block, confirmations should change:
        self.nodes[0].generate(1)
        self.sync_all()
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid},
                           {"category": "send", "amount": Decimal("-0.1"), "confirmations": 1})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"txid": txid},
                           {"category": "receive", "amount": Decimal("0.1"), "confirmations": 1})

        # send-to-self:
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid, "category": "send"},
                           {"amount": Decimal("-0.2")})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid, "category": "receive"},
                           {"amount": Decimal("0.2")})

        # sendmany from node1: twice to self, twice to node2:
        send_to = {self.nodes[0].getnewaddress(): 0.11,
                   self.nodes[1].getnewaddress(): 0.22,
                   self.nodes[0].getnewaddress(): 0.33,
                   self.nodes[1].getnewaddress(): 0.44}
        txid = self.nodes[1].sendmany("", send_to)
        self.sync_all()
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category":"send", "amount": Decimal("-0.11")},
                           {"txid": txid})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.11")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.22")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.22")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.33")},
                           {"txid": txid})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.33")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.44")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.44")},
                           {"txid": txid})

        multisig = self.nodes[1].createmultisig(1, [self.nodes[1].getnewaddress()])
        self.nodes[0].importaddress(multisig["redeemScript"], "watchonly", False, True)
        txid = self.nodes[1].sendtoaddress(multisig["address"], 0.1)
        self.nodes[1].generate(1)
        self.sync_all()
        assert not [tx for tx in self.nodes[0].listtransactions("*", 100, 0, False) if "label" in tx and tx["label"] == "watchonly"]
        txs = [tx for tx in self.nodes[0].listtransactions("*", 100, 0, True) if "label" in tx and tx['label'] == 'watchonly']
        assert_array_result(txs, {"category": "receive", "amount": Decimal("0.1")}, {"txid": txid})

        # Send 10 HMS with subtract fee from amount
        node_0_bal = self.nodes[0].getbalance()
        node_1_bal = self.nodes[1].getbalance()
        self.log.info("test sendtoaddress with subtract-fee-from-amt")
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 10, "", "", True)
        node_0_bal -= Decimal('10')
        assert_equal(self.nodes[0].getbalance(), node_0_bal)
        self.nodes[0].generate(1)
        self.sync_all()
        fee = self.nodes[0].gettransaction(txid)["fee"]     # fee < 0
        node_1_bal += (Decimal('10') + fee)
        assert_equal(self.nodes[1].getbalance(), node_1_bal)
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid},
                           {"category": "send", "amount": - Decimal('10') - fee, "confirmations": 1})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"txid": txid},
                           {"category": "receive", "amount": + Decimal('10') + fee, "confirmations": 1})

        # Sendmany 10 HMS with subtract fee from amount
        node_0_bal = self.nodes[0].getbalance()
        node_1_bal = self.nodes[1].getbalance()
        self.log.info("test sendmany with subtract-fee-from-amt")
        address = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendmany('', {address: 10}, 1, "", False, [address])
        node_0_bal -= Decimal('10')
        assert_equal(self.nodes[0].getbalance(), node_0_bal)
        self.nodes[0].generate(1)
        self.sync_all()
        fee = self.nodes[0].gettransaction(txid)["fee"]     # fee < 0
        node_1_bal += (Decimal('10') + fee)
        assert_equal(self.nodes[1].getbalance(), node_1_bal)
        assert_array_result(self.nodes[0].listtransactions(),
                            {"txid": txid},
                            {"category": "send", "amount": - Decimal('10') - fee, "confirmations": 1})
        assert_array_result(self.nodes[1].listtransactions(),
                            {"txid": txid},
                            {"category": "receive", "amount": + Decimal('10') + fee, "confirmations": 1})

if __name__ == '__main__':
    ListTransactionsTest().main()
