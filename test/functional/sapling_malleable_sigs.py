#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from io import BytesIO

from test_framework.messages import CTransaction
from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    hex_str_to_bytes,
)


class MalleableSigsTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        saplingUpgrade = ['-nuparams=v5_shield:201']
        self.extra_args = [saplingUpgrade]

    def run_test(self):
        node = self.nodes[0]
        node.generate(2)
        assert_equal(node.getblockcount(), 202)

        z_addr = node.getnewshieldaddress()
        shield_to = [{"address": z_addr, "amount": Decimal('10')}]

        # Create rawtx shielding 10 PIV
        self.log.info("Shielding 10 PIV...")
        rawtx_hex = node.rawshieldsendmany("from_transparent", shield_to)
        self.log.info("Raw tx created")

        # Creating malleated tx
        self.log.info("Removing sapling data...")
        new_tx = CTransaction()
        new_tx.deserialize(BytesIO(hex_str_to_bytes(rawtx_hex)))
        new_tx.sapData = b""
        new_rawtx = bytes_to_hex_str(new_tx.serialize())
        self.log.info("Sending malleated tx...")
        assert_raises_rpc_error(-26, "mandatory-script-verify-flag-failed",
                                node.sendrawtransaction, new_rawtx, True)
        self.log.info("Good. Tx NOT accepted in mempool")



if __name__ == '__main__':
    MalleableSigsTest().main()
