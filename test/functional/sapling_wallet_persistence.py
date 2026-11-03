#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 20202 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
import os
import shutil

from test_framework.test_framework import PivxTestFramework
from test_framework.util import assert_equal, assert_true

class SaplingWalletPersistenceTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-nuparams=v5_shield:1', '-whitelist=127.0.0.1']] * self.num_nodes

    def restart_watcher(self):
        self.stop_node(1)
        self.start_node(1)
        self.connect_nodes(1, 0)

    def pay(self, from_addr, to_addr, amount):
        self.nodes[0].shieldsendmany(from_addr, [{"address": to_addr, "amount": Decimal(amount)}], 1)
        self.sync_mempools()
        self.nodes[0].generate(1)
        self.sync_all()

    def assert_watcher_follows(self, addr):
        assert_equal(self.nodes[1].getshieldbalance(addr, 1, True), self.nodes[0].getshieldbalance(addr))

    def test_viewing_key(self, funded):
        miner, watcher = self.nodes
        addr = miner.getnewshieldaddress()
        out = miner.getnewaddress()
        watcher.importsaplingviewingkey(miner.exportsaplingviewingkey(addr), "no")
        if funded:
            self.pay("from_transparent", addr, 10)

        for _ in range(3):
            self.restart_watcher()
            assert_true(addr in watcher.listshieldaddresses(True), "Should contain address after restart")
            self.assert_watcher_follows(addr)
            self.pay("from_transparent", addr, 5)
            assert_true(miner.getshieldbalance(addr) >= Decimal(5))
            self.assert_watcher_follows(addr)
            self.pay(addr, out, 1)
            self.assert_watcher_follows(addr)
        return addr

    def test_viewing_key_encrypted(self, addr):
        watcher = self.nodes[1]
        watcher.encryptwallet("passphrase")
        self.restart_watcher()
        assert_true(addr in watcher.listshieldaddresses(True), "Should contain address after encryption")
        self.assert_watcher_follows(addr)
        self.pay("from_transparent", addr, 1)
        self.assert_watcher_follows(addr)

    def new_watcher_wallet(self):
        self.stop_node(1)
        wallets = os.path.join(self.nodes[1].datadir, "regtest", "wallets")
        shutil.rmtree(wallets)
        os.mkdir(wallets)
        self.start_node(1)
        self.connect_nodes(1, 0)

    def test_rescan_in_new_wallet(self, addr):
        miner, watcher = self.nodes
        for import_key in [lambda: watcher.importsaplingviewingkey(miner.exportsaplingviewingkey(addr), "yes"),
                           lambda: watcher.importsaplingkey(miner.exportsaplingkey(addr), "yes")]:
            self.new_watcher_wallet()
            import_key()
            self.assert_watcher_follows(addr)
            miner.generate(2)
            self.sync_all()
            self.restart_watcher()
            self.assert_watcher_follows(addr)

    def run_test(self):
        sapling_addr = self.nodes[0].getnewshieldaddress()
        addresses = self.nodes[0].listshieldaddresses()
        # make sure the node has the addresses
        assert_true(sapling_addr in addresses, "Should contain address before restart")
        # restart the nodes
        self.stop_node(0)
        self.start_node(0)
        self.connect_nodes(0, 1)
        addresses = self.nodes[0].listshieldaddresses()
        # make sure we still have the address after restarting
        assert_true(sapling_addr in addresses, "Should contain address after restart")

        self.nodes[0].generate(110)
        self.sync_all()

        # Used to die at the restart, on the first note a rescan met
        self.log.info("An imported viewing key and its notes survive restarts, and it finds new ones")
        addr = self.test_viewing_key(True)
        # Used to go unnoticed: with nothing to meet, the node started and no longer saw the address
        self.log.info("The same for a viewing key that has no notes yet")
        self.test_viewing_key(False)

        self.log.info("It survives encrypting the wallet too")
        self.test_viewing_key_encrypted(addr)

        # Used to trip an assert at the restart: the witnesses were saved, their cache size was not
        self.log.info("A new wallet on a running chain survives a restart after a rescan over its notes")
        self.test_rescan_in_new_wallet(addr)

if __name__ == '__main__':
    SaplingWalletPersistenceTest().main()
