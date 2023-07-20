#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_false,
    assert_raises_rpc_error,
    assert_true,
    bytes_to_hex_str
)

my_memo_str = "What, so everyone’s supposed to sleep every single night now?\n"\
              "You realize that nighttime makes up half of all time?"
my_memo_hex = bytes_to_hex_str(my_memo_str.encode('utf-8'))

non_ascii_memo_str = "零知识证明"
non_ascii_memo_hex = bytes_to_hex_str(non_ascii_memo_str.encode('utf-8'))

too_big_memo_str = "This is not an email......." * 19
no_memo = "f6"


class ListReceivedTest (PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 4
        saplingUpgrade = ['-nuparams=v5_shield:201']
        self.extra_args = [saplingUpgrade, saplingUpgrade, saplingUpgrade, saplingUpgrade]

    def generate_and_sync(self, new_height):
        current_height = self.nodes[0].getblockcount()
        assert(new_height > current_height)
        self.nodes[0].generate(new_height - current_height)
        self.sync_all()
        assert_equal(new_height, self.nodes[0].getblockcount())

    def run_test(self):
        height = 214
        self.generate_and_sync(height+1)
        taddr = self.nodes[1].getnewaddress()
        shield_addr1 = self.nodes[1].getnewshieldaddress()
        shield_addrExt = self.nodes[3].getnewshieldaddress()

        self.nodes[0].sendtoaddress(taddr, 6.0) # node_1 in taddr with 6 PIV.
        self.generate_and_sync(height+2)

        # Try to send with an oversized memo
        assert_raises_rpc_error(-4, "Memo size of 513 is too big, maximum allowed is 512",
                                self.nodes[1].shieldsendmany, taddr,
                                [{'address': shield_addr1, 'amount': 2, 'memo': too_big_memo_str}])
        # Fixed fee
        fee = 0.05

        # Send 1 PIV to shield addr1
        txid = self.nodes[1].shieldsendmany(taddr, [ # node_1 with 6 PIV sending them all (fee is 0.1 PIV)
            {'address': shield_addr1, 'amount': 2, 'memo': my_memo_str},
            {'address': shield_addrExt, 'amount': 3},
        ], 1, fee)
        self.sync_all()

        # Decrypted transaction details should be correct
        pt = self.nodes[1].viewshieldtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 0)
        assert_equal(len(pt['outputs']), 2)

        found = [False, False]
        for out in pt['outputs']:
            assert_equal(pt['outputs'].index(out), out['output'])
            if out['address'] == shield_addr1:
                assert_equal(out['outgoing'], False)
                assert_equal(out['memo'], my_memo_hex)
                assert_equal(out['memoStr'], my_memo_str)
                assert_equal(out['value'], Decimal('2'))
                assert_equal(out['valueSat'], 200000000)
                found[0] = True
            else:
                assert_equal(out['address'], shield_addrExt)
                assert_equal(out['outgoing'], True)
                assert_equal(out['memo'], no_memo)
                assert_equal(out['value'], Decimal('3'))
                assert_equal(out['valueSat'], 300000000)
                found[1] = True
        assert_equal(found, [True] * 2)

        r = self.nodes[1].listreceivedbyshieldaddress(shield_addr1)
        assert_true(0 == len(r), "Should have received no confirmed note")
        c = self.nodes[1].getsaplingnotescount()
        assert_true(0 == c, "Count of confirmed notes should be 0")

        # No confirmation required, one note should be present
        r = self.nodes[1].listreceivedbyshieldaddress(shield_addr1, 0)
        assert_true(1 == len(r), "Should have received one (unconfirmed) note")
        assert_equal(txid, r[0]['txid'])
        assert_equal(2, r[0]['amount'])
        assert_false(r[0]['change'], "Note should not be change")
        assert_equal(my_memo_hex, r[0]['memo'])
        assert_equal(0, r[0]['confirmations'])
        assert_equal(0, r[0]['blockindex'])
        assert_equal(0, r[0]['blockheight'])

        c = self.nodes[1].getsaplingnotescount(0)
        assert_true(1 == c, "Count of unconfirmed notes should be 1")

        # Confirm transaction (2 PIV from taddr to shield_addr1)
        self.generate_and_sync(height+3)

        # adjust confirmations
        r[0]['confirmations'] = 1
        # adjust blockindex
        r[0]['blockindex'] = 1
        # adjust height
        r[0]['blockheight'] = height + 3

        # Require one confirmation, note should be present
        r2 = self.nodes[1].listreceivedbyshieldaddress(shield_addr1)
        # As time will be different (tx was included in a block), need to remove it from the dict
        assert_true(r[0]['blocktime'] != r2[0]['blocktime'])
        del r[0]['blocktime']
        del r2[0]['blocktime']
        # Now can check that the information is the same
        assert_equal(r, r2)

        # Get the note nullifier
        lsu = self.nodes[1].listshieldunspent()
        assert_equal(len(lsu), 1)
        nullifier = lsu[0]["nullifier"]

        # Generate some change by sending part of shield_addr1 to shield_addr2
        txidPrev = txid
        shield_addr2 = self.nodes[1].getnewshieldaddress()
        txid = self.nodes[1].shieldsendmany(shield_addr1, # shield_addr1 has 2 PIV, send 0.6 PIV + 0.05 PIV fee
                                               [{'address': shield_addr2, 'amount': 0.6, "memo": non_ascii_memo_str}],
                                               1, fee) # change 1.35
        self.sync_all()
        self.generate_and_sync(height+4)

        # Verify the spent nullifier
        tx_json = self.nodes[1].getrawtransaction(txid, True)
        assert_equal(nullifier, tx_json["vShieldSpend"][0]["nullifier"])

        # Decrypted transaction details should be correct
        pt = self.nodes[1].viewshieldtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 1)
        assert_equal(len(pt['outputs']), 2)

        assert_equal(pt['spends'][0]['txidPrev'], txidPrev)
        assert_equal(pt['spends'][0]['spend'], 0)
        assert_equal(pt['spends'][0]['outputPrev'], 0)
        assert_equal(pt['spends'][0]['address'], shield_addr1)
        assert_equal(pt['spends'][0]['value'], Decimal('2.0'))
        assert_equal(pt['spends'][0]['valueSat'], 200000000)

        found = [False, False]
        for out in pt['outputs']:
            assert_equal(pt['outputs'].index(out), out['output'])
            if out['address'] == shield_addr2:
                assert_equal(out['outgoing'], False)
                assert_equal(out['memo'], non_ascii_memo_hex)
                assert_equal(out['memoStr'], non_ascii_memo_str)
                assert_equal(out['value'], Decimal('0.6'))
                assert_equal(out['valueSat'], 60000000)
                found[0] = True
            else:
                assert_equal(out['address'], shield_addr1)
                assert_equal(out['outgoing'], False)
                assert_equal(out['memo'], no_memo)
                assert_equal(out['value'], Decimal('1.35'))
                assert_equal(out['valueSat'], 135000000)
                found[1] = True
        assert_equal(found, [True] * 2)

        # shield_addr1 should have a note with change
        r = self.nodes[1].listreceivedbyshieldaddress(shield_addr1, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_true(2 == len(r), "shield_addr1 Should have received 2 notes")

        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('1.35'), r[0]['amount'])
        assert_true(r[0]['change'], "Note valued at (1.4-fee) should be change")
        assert_equal(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        assert_equal(Decimal('2.0'), r[1]['amount'])
        assert_false(r[1]['change'], "Note valued at 2.0 should not be change")
        assert_equal(my_memo_hex, r[1]['memo'])

        # shield_addr2 should not have change
        r = self.nodes[1].listreceivedbyshieldaddress(shield_addr2, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_true(1 == len(r), "shield_addr2 Should have received 1 notes")
        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.6'), r[0]['amount'])
        assert_false(r[0]['change'], "Note valued at 0.6 should not be change")
        assert_equal(non_ascii_memo_hex, r[0]['memo'])

        c = self.nodes[1].getsaplingnotescount(0)
        assert_true(3 == c, "Count of unconfirmed notes should be 3(2 in shield_addr1 + 1 in shield_addr2)")

if __name__ == '__main__':
    ListReceivedTest().main()
