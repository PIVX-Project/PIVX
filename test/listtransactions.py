#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Exercise the listtransactions API

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import AuthServiceProxy, JSONRPCException
from test_framework.util import *
from test_case_base import TestCaseBase


def check_array_result(object_array, to_match, expected):
    """
    Pass in array of JSON objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    """
    num_matched = 0
    for item in object_array:
        all_match = True
        for key,value in to_match.items():
            if item[key] != value:
                all_match = False
        if not all_match:
            continue
        for key,value in expected.items():
            if item[key] != value:
                raise AssertionError("%s : expected %s=%s got %s" % (str(item), str(key), str(value), str(item[key])))
            num_matched = num_matched+1
    if num_matched == 0:
        raise AssertionError("No objects matched %s"%(str(to_match)))

class ListTransactionsTest(TestCaseBase) :
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-keypool=10"]] * self.num_nodes
        self.enable_mocktime()

    def initialize(self) :
        pass

    def test_simple_send(self):
        # Simple send, 0 to 1:
        #print("====== ", self.nodes[0].getblockcount(), " === ", self.nodes[0].listtransactions())
        self.sync_all()
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        #for i in self.nodes[0].listtransactions("", 1000):
        #    print(i["txid"], i["amount"])
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"send","account":"","amount":Decimal("-0.1"),"confirmations":0})
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"receive","account":"","amount":Decimal("0.1"),"confirmations":0})

        self.nodes[1].setgenerate(True, 1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"send","account":"","amount":Decimal("-0.1"),"confirmations":1})
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"receive","account":"","amount":Decimal("0.1"),"confirmations":1})

        # mine a block, confirmations should change:
        self.nodes[0].setgenerate(True, 1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"send","account":"","amount":Decimal("-0.1"),"confirmations":2})
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"txid":txid},
                           {"category":"receive","account":"","amount":Decimal("0.1"),"confirmations":2})

    def test_send_to_self(self):
        # send-to-self:
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"txid":txid, "category":"send"},
                           {"amount":Decimal("-0.2")})
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"txid":txid, "category":"receive"},
                           {"amount":Decimal("0.2")})

    def test_send_many(self):
        # sendmany from node1: twice to self, twice to node2:
        send_to = {
            self.nodes[0].getnewaddress() : 0.11,
            self.nodes[1].getnewaddress() : 0.22
        }
        txid = self.nodes[1].sendmany("", send_to)
        self.sync_all()
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"category":"send","amount":Decimal("-0.11")},
                           {"txid":txid} )
        check_array_result(self.nodes[0].listtransactions("", 1000),
                           {"category":"receive","amount":Decimal("0.11")},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"category":"send","amount":Decimal("-0.22")},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions("", 1000),
                           {"category":"receive","amount":Decimal("0.22")},
                           {"txid":txid} )

if __name__ == '__main__':
    ListTransactionsTest().main()

