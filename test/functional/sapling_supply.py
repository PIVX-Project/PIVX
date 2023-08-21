#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
)

class SaplingSupplyTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-nuparams=v5_shield:1']]

    def generate_and_sync(self, count):
        assert(count > 0)
        height = self.nodes[0].getblockcount()
        self.nodes[0].generate(count)
        assert_equal(height + count, self.nodes[0].getblockcount())

    def check_shield_supply(self, z_supply):
        self.log.info("Checking supply...")
        assert_equal(self.nodes[0].getsupplyinfo()['shieldsupply'], Decimal("%.8f" % z_supply))
        self.log.info("OK. Shield supply is %.8f" % z_supply)

    def run_test(self):
        fee = 0.05
        # First mine 101 blocks to mature one utxo
        self.log.info("Generating 101 blocks...")
        self.generate_and_sync(101)

        # Check the shield supply 0
        z_supply = 0
        self.check_shield_supply(z_supply)

        # Send 200 HMS to shield addr1
        self.log.info("Shielding 200 HMS...")
        z_addr1 = self.nodes[0].getnewshieldaddress()
        txid = self.nodes[0].shieldsendmany(
            "from_transparent", [{'address': z_addr1, 'amount': 200,
                                  'memo': "first shield coin!"}], 1, fee)
        self.sync_all()

        # Decrypted transaction details should be correct
        pt = self.nodes[0].viewshieldtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 0)
        assert_equal(len(pt['outputs']), 1)
        out = pt['outputs'][0]
        assert_equal(out['output'], 0)
        assert_equal(out['address'], z_addr1)
        assert_equal(out['outgoing'], False)
        assert_equal(out['memoStr'], "first shield coin!")
        assert_equal(out['value'], Decimal('200'))
        assert_equal(out['valueSat'], 20000000000)

        # Check the shield supply
        self.generate_and_sync(5)
        z_supply += 200
        self.check_shield_supply(z_supply)

        # Deshield 100 coins
        self.log.info("Deshielding 100 HMS...")
        t_addr1 = self.nodes[0].getnewaddress()
        txid = self.nodes[0].shieldsendmany(
            "from_shield", [{'address': t_addr1, 'amount': 100}], 1, fee)
        self.sync_all()

        # Decrypted transaction details should be correct
        pt = self.nodes[0].viewshieldtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 1)
        assert_equal(len(pt['outputs']), 1)

        # Check the shield supply
        self.generate_and_sync(1)
        z_supply -= (100 + fee)
        self.check_shield_supply(z_supply)

        # Disconnect the block and check supply
        self.log.info("Disconnecting last block...")
        best_block = self.nodes[0].getbestblockhash()
        self.nodes[0].invalidateblock(best_block)
        z_supply += (100 + fee)
        self.check_shield_supply(z_supply)

        # Reconnect the block and check supply again
        self.log.info("Reconnecting last block...")
        self.nodes[0].reconsiderblock(best_block)
        z_supply -= (100 + fee)
        self.check_shield_supply(z_supply)


if __name__ == '__main__':
    SaplingSupplyTest().main()
