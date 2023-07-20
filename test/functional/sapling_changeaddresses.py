#!/usr/bin/env python3
# Copyright (c) 2019 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal

from test_framework.test_framework import PivxTestFramework
from test_framework.util import assert_greater_than_or_equal, get_coinstake_address


# Test wallet change address behaviour
class WalletChangeAddressesTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        saplingUpgrade = ['-nuparams=v5_shield:1']
        self.extra_args = [saplingUpgrade]

    def run_test(self):
        node = self.nodes[0]
        node.generate(110)

        # Obtain some transparent funds
        midAddr = node.getnewshieldaddress()
        # Shield almost all the balance
        node.shieldsendmany(get_coinstake_address(node), [{"address": midAddr, "amount": Decimal(2400)}])
        node.generate(1)
        taddrSource = node.getnewaddress()
        for _ in range(6):
            recipients = [{"address": taddrSource, "amount": Decimal('3')}]
            node.shieldsendmany(midAddr, recipients, 1)
            node.generate(1)

        def check_change_taddr_reuse(target):
            recipients = [{"address": target, "amount": Decimal('1')}]

            # Send funds to recipient address twice
            txid1 = node.shieldsendmany(taddrSource, recipients, 1)
            node.generate(1)
            txid2 = node.shieldsendmany(taddrSource, recipients, 1)
            node.generate(1)

            # Verify that the two transactions used different change addresses
            tx1 = node.getrawtransaction(txid1, 1)
            tx2 = node.getrawtransaction(txid2, 1)
            assert_greater_than_or_equal(len(tx1['vout']), 1) # at least one output
            assert_greater_than_or_equal(len(tx2['vout']), 1)
            for i in range(len(tx1['vout'])):
                tx1OutAddrs = tx1['vout'][i]['scriptPubKey']['addresses'][0]
                tx2OutAddrs = tx2['vout'][i]['scriptPubKey']['addresses'][0]
                if tx1OutAddrs != target:
                    self.log.info('Source address:     %s' % taddrSource)
                    self.log.info('TX1 change address: %s' % tx1OutAddrs)
                    self.log.info('TX2 change address: %s' % tx2OutAddrs)
                    assert tx1OutAddrs != tx2OutAddrs

        taddr = node.getnewaddress()
        saplingAddr = node.getnewshieldaddress()

        self.log.info('Checking shieldsendmany(taddr->Sapling)')
        check_change_taddr_reuse(saplingAddr)
        self.log.info('Checking shieldsendmany(taddr->taddr)')
        check_change_taddr_reuse(taddr)

if __name__ == '__main__':
    WalletChangeAddressesTest().main()
