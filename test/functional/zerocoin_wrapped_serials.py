#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the 'Wrapped Serials Attack' scenario
'''

import random
from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than

from fake_stake.base_test import PIVX_FakeStakeTest

class zPIVwrappedSerialsTest(PIVX_FakeStakeTest):

    def run_test(self):
        q = 73829871667027927151400291810255409637272593023945445234219354687881008052707
        pow2 = 2**256
        self.description = "Covers the 'Wrapped Serials Attack' scenario."
        self.init_test()

        INITAL_MINED_BLOCKS = 351   # Blocks mined before minting
        MORE_MINED_BLOCKS = 31      # Blocks mined after minting (before spending)
        DENOM_TO_USE = 1000         # zc denomination used for double spending attack
        K_BITSIZE = 128             # bitsize of the range for random K
        NUM_OF_K = 5                # number of wrapping serials to try

        # 1) Start mining blocks
        self.log.info("Mining %d first blocks..." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)
        sleep(2)

        # 2) Mint zerocoins
        self.log.info("Minting %d-denom zPIVs..." % DENOM_TO_USE)
        balance = self.node.getbalance("*", 100)
        assert_greater_than(balance, DENOM_TO_USE)
        total_mints = 0
        while balance > DENOM_TO_USE:
            try:
                self.node.mintzerocoin(DENOM_TO_USE)
            except JSONRPCException:
                break
            sleep(1)
            total_mints += 1
            self.node.generate(1)
            sleep(1)
            if total_mints % 5 == 0:
                self.log.info("Minted %d coins" % total_mints)
            if total_mints >= 20:
                break
            balance = self.node.getbalance("*", 100)
        sleep(2)

        # 3) Mine more blocks and collect the mint
        self.log.info("Mining %d more blocks..." % MORE_MINED_BLOCKS)
        self.node.generate(MORE_MINED_BLOCKS)
        sleep(2)
        mint = self.node.listmintedzerocoins(True, True)[0]

        # 4) Get the raw zerocoin data
        exported_zerocoins = self.node.exportzerocoins(False)
        zc = [x for x in exported_zerocoins if mint["serial hash"] == x["id"]]
        if len(zc) == 0:
            raise AssertionError("mint not found")

        # 5) Spend the minted coin (mine two more blocks)
        self.log.info("Spending the minted coin with serial %s and mining two more blocks..." % zc[0]["s"])
        txid = self.node.spendzerocoinmints([mint["serial hash"]])['txid']
        self.log.info("Spent on tx %s" % txid)
        self.node.generate(2)
        sleep(2)

        # 6) create the new serials
        new_serials = []
        for i in range(NUM_OF_K):
            K = random.getrandbits(K_BITSIZE)
            new_serials.append(hex(int(zc[0]["s"], 16) + K*q*pow2)[2:])

        randomness = zc[0]["r"]
        privkey = zc[0]["k"]

        # 7) Spend the new zerocoins
        for serial in new_serials:
            self.log.info("Spending the wrapping serial %s" % serial)
            tx = None
            try:
                tx = self.node.spendrawzerocoin(serial, randomness, DENOM_TO_USE, privkey)
            except JSONRPCException as e:
                exc_msg = str(e)
                if exc_msg == "CoinSpend: failed check (-4)":
                    self.log.info("GOOD: Transaction did not verify")
                else:
                    raise e

            if tx is not None:
                self.log.warning("Tx is: %s" % tx)
                raise AssertionError("TEST FAILED")

        self.log.info("%s PASSED" % self.__class__.__name__)



if __name__ == '__main__':
    zPIVwrappedSerialsTest().main()
