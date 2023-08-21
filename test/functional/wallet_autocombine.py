#!/usr/bin/env python3
# Copyright (c) 2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet's autocombine feature."""

import time

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)


class AutoCombineTest(HemisTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # Check that there's no UTXOs
        assert_equal(len(self.nodes[0].listunspent()), 0)

        # Check the failure conditions for setautocombinethreshold
        assert_raises_rpc_error(-8, "Missing threshold value", self.nodes[0].setautocombinethreshold, True)
        assert_raises_rpc_error(-8, "The threshold value cannot be less than 1.00", self.nodes[0].setautocombinethreshold, True, 0.99)
        assert_raises_rpc_error(-8, "Frequency must be greater than 0", self.nodes[0].setautocombinethreshold, True, 2, -1)
        assert_raises_rpc_error(-8, "The threshold value cannot be less than 1.00", self.nodes[0].setautocombinethreshold, True, 0.99)

        self.log.info("Mining initial 100 blocks...")
        self.nodes[0].generate(100)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 25000)
        assert_equal(walletinfo['balance'], 0)

        self.log.info("Mining 2 more blocks to use as autocombine inputs")
        self.nodes[0].generate(2)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250*2)
        assert_equal(walletinfo['txcount'], 102)

        self.log.info("Set autocombine to 500 HMS and frequency of 10")
        setautocombine = self.nodes[0].setautocombinethreshold(True, 500, 10)
        assert_equal(setautocombine['enabled'], True)
        assert_equal(setautocombine['threshold'], 500)
        assert_equal(setautocombine['frequency'], 10)
        getautocombine = self.nodes[0].getautocombinethreshold()
        assert_equal(getautocombine['enabled'], True)
        assert_equal(getautocombine['threshold'], 500)
        assert_equal(getautocombine['frequency'], 10)
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['autocombine_enabled'], True)
        assert_equal(walletinfo['autocombine_threshold'], 500)
        assert_equal(walletinfo['autocombine_frequency'], 10)

        self.log.info("Mine 8 more block to initiate an autocombine transaction")

        self.nodes[0].generate(8)  # we need 8 more blocks to get a multiple of 10
        assert_equal(0, self.nodes[0].getblockcount() % 10)
        time.sleep(1)

        mempool = self.nodes[0].getrawmempool()
        assert_equal(len(mempool), 1)
        tx = mempool[0]
        nFee = self.nodes[0].getrawmempool(True)[mempool[0]]['fee']
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250*10 - nFee)
        assert_equal(walletinfo['txcount'], 111)  # 110 coinbase txs + 1 to autocollect

        self.log.info("Mine 1 more block to confirm the autocombine transaction")
        block = self.nodes[0].generate(1)
        time.sleep(1)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250*11 - nFee)
        # 111 coinbase txs + 1 autocollect, good autocollect was not called again since nBlock % 10 = 1
        assert_equal(1, self.nodes[0].getblockcount() % 10)
        assert_equal(walletinfo['txcount'], 112)

        txinfo = self.nodes[0].gettransaction(tx)
        assert_equal(txinfo['fee'], 0 - nFee)
        assert_equal(txinfo['confirmations'], 1)
        assert_equal(txinfo['amount'], 0)
        assert_equal(txinfo['blockhash'], block[0])

        self.log.info("Disable autocombine")
        setautocombine = self.nodes[0].setautocombinethreshold(False)
        assert_equal(setautocombine['enabled'], False)
        assert_equal(setautocombine['threshold'], 0)
        assert_equal(setautocombine['frequency'], 30)  # set back to default value

        getautocombine = self.nodes[0].getautocombinethreshold()
        assert_equal(getautocombine['enabled'], False)
        assert_equal(getautocombine['threshold'], 0)
        assert_equal(getautocombine['frequency'], 30)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['autocombine_enabled'], False)
        assert_equal(walletinfo['autocombine_threshold'], 0)
        assert_equal(walletinfo['autocombine_frequency'], 30)

        self.log.info("Mine 1 more block to make sure autocombine is disabled")
        self.nodes[0].generate(1)
        time.sleep(1)

        mempool = self.nodes[0].getrawmempool()
        assert_equal(len(mempool), 0)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['balance'], 250*12 - nFee)
        assert_equal(walletinfo['txcount'], 113)


if __name__ == '__main__':
    AutoCombineTest().main()
