#!/usr/bin/env python3
# Copyright (c) 2021 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet's autocombine feature."""

import time

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)


class AutoCombineTest(PivxTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # Check that there's no UTXOs
        assert_equal(len(self.nodes[0].listunspent()), 0)

        # Check the failure conditions for setautocombinethreshold
        assert_raises_rpc_error(-8, "Missing threshold value", self.nodes[0].setautocombinethreshold, True)
        assert_raises_rpc_error(-8, "The threshold value cannot be less than 1.00", self.nodes[0].setautocombinethreshold, True, 0.99)

        self.log.info("Mining initial 100 blocks...")
        self.nodes[0].generate(100)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 25000)
        assert_equal(walletinfo['balance'], 0)

        self.log.info("Mining 2 more blocks to use as autocombine inputs")
        self.nodes[0].generate(2)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 500)
        assert_equal(walletinfo['txcount'], 102)

        self.log.info("Set autocombine to 500 PIV")
        setautocombine = self.nodes[0].setautocombinethreshold(True, 500)
        assert_equal(setautocombine['enabled'], True)
        assert_equal(setautocombine['threshold'], 500)
        getautocombine = self.nodes[0].getautocombinethreshold()
        assert_equal(getautocombine['enabled'], True)
        assert_equal(getautocombine['threshold'], 500)
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['autocombine_enabled'], True)
        assert_equal(walletinfo['autocombine_threshold'], 500)

        self.log.info("Mine 1 more block to initiate an autocombine transaction")
        self.nodes[0].generate(1)
        time.sleep(1)

        mempool = self.nodes[0].getrawmempool()
        assert_equal(len(mempool), 1)
        tx = mempool[0]
        nFee = self.nodes[0].getrawmempool(True)[mempool[0]]['fee']
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 750 - nFee)
        assert_equal(walletinfo['txcount'], 104)

        self.log.info("Mine 1 more block to confirm the autocombine transaction")
        block = self.nodes[0].generate(1)
        time.sleep(1)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250 + 750 - nFee)
        assert_equal(walletinfo['txcount'], 105)

        txinfo = self.nodes[0].gettransaction(tx)
        assert_equal(txinfo['fee'], 0 - nFee)
        assert_equal(txinfo['confirmations'], 1)
        assert_equal(txinfo['amount'], 0)
        assert_equal(txinfo['blockhash'], block[0])

        self.log.info("Disable autocombine")
        setautocombine = self.nodes[0].setautocombinethreshold(False)
        assert_equal(setautocombine['enabled'], False)
        assert_equal(setautocombine['threshold'], 0)
        getautocombine = self.nodes[0].getautocombinethreshold()
        assert_equal(getautocombine['enabled'], False)
        assert_equal(getautocombine['threshold'], 0)
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['autocombine_enabled'], False)
        assert_equal(walletinfo['autocombine_threshold'], 0)

        self.log.info("Mine 1 more block to make sure autocombine is disabled")
        self.nodes[0].generate(1)
        time.sleep(1)

        mempool = self.nodes[0].getrawmempool()
        assert_equal(len(mempool), 0)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250 + 250 + 750 - nFee)
        assert_equal(walletinfo['txcount'], 106)


if __name__ == '__main__':
    AutoCombineTest().main()
