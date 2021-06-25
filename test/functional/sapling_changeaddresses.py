#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Copyright (c) 2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal

from test_framework.test_framework import PivxTestFramework
from test_framework.util import assert_true, get_coinstake_address


# Test wallet change address behaviour
class WalletChangeAddressesTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        saplingUpgrade = ['-nuparams=v5_shield:1']
        self.extra_args = [saplingUpgrade, saplingUpgrade]

    def run_test(self):
        self.nodes[0].generate(110)

        # Obtain some transparent funds
        midAddr = self.nodes[0].getnewshieldaddress()
        # Shield almost all the balance
        self.nodes[0].shieldsendmany(get_coinstake_address(self.nodes[0]), [{"address": midAddr, "amount": Decimal(2400)}])

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        taddrSource = self.nodes[0].getnewaddress()
        for _ in range(6):
            recipients = [{"address": taddrSource, "amount": Decimal('3')}]
            self.nodes[0].shieldsendmany(midAddr, recipients, 1)
            self.sync_all()
            self.nodes[1].generate(1)
            self.sync_all()

        def check_change_taddr_reuse(target, isTargetShielded):
            recipients = [{"address": target, "amount": Decimal('1')}]

            # Send funds to recipient address twice
            txid1 = self.nodes[0].shieldsendmany(taddrSource, recipients, 1)
            self.nodes[1].generate(1)
            self.sync_all()
            txid2 = self.nodes[0].shieldsendmany(taddrSource, recipients, 1)
            self.nodes[1].generate(1)
            self.sync_all()

            # Verify that the two transactions used different change addresses
            tx1 = self.nodes[0].getrawtransaction(txid1, 1)
            tx2 = self.nodes[0].getrawtransaction(txid2, 1)
            assert_true(len(tx1['vout']) >= 1) # at least one output
            assert_true(len(tx2['vout']) >= 1)
            for i in range(len(tx1['vout'])):
                tx1OutAddrs = tx1['vout'][i]['scriptPubKey']['addresses']
                tx2OutAddrs = tx2['vout'][i]['scriptPubKey']['addresses']
                if tx1OutAddrs != [target]:
                    print('Source address:     %s' % taddrSource)
                    print('TX1 change address: %s' % tx1OutAddrs[0])
                    print('TX2 change address: %s' % tx2OutAddrs[0])
                    assert(tx1OutAddrs != tx2OutAddrs)

        taddr = self.nodes[0].getnewaddress()
        saplingAddr = self.nodes[0].getnewshieldaddress()

        print()
        print('Checking shieldsendmany(taddr->Sapling)')
        check_change_taddr_reuse(saplingAddr, True)
        print()
        print('Checking shieldsendmany(taddr->taddr)')
        check_change_taddr_reuse(taddr, False)

if __name__ == '__main__':
    WalletChangeAddressesTest().main()
