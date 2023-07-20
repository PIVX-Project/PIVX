#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import time

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_equal,
    Decimal,
    satoshi_round,
)


def timed(f):
  start = time.time()
  ret = f()
  elapsed = time.time() - start
  return ret, elapsed

MAX_SHIELDED_BLOCKSIZE = 750000

class SaplingFillBlockTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-blockmaxsize=1999000"], []]

    def utxo_splitter(self, node_from, n_inputs, node_to):
        txids = []
        # collect utxos
        utxos = node_from.listunspent()
        assert_greater_than_or_equal(len(utxos), n_inputs)
        # sort by size
        sorted(utxos, key=lambda utxo: utxo["amount"], reverse=True)
        # pick the first N
        utxos = utxos[:n_inputs]
        # split each one in 100 (use fixed 0.05 PIV fee)
        for u in utxos:
            prevout = [{"txid": u["txid"], "vout": u["vout"]}]
            output_amt = satoshi_round((u["amount"] - Decimal("0.05")) / 100)
            recipients = {node_to.getnewaddress(): output_amt for _ in range(100)}
            rawTx_unsigned = node_from.createrawtransaction(prevout, recipients)
            rawTx_signed = node_from.signrawtransaction(rawTx_unsigned)["hex"]
            txids.append(node_from.sendrawtransaction(rawTx_signed))
        return txids

    def check_mempool(self, miner, txids):
        self.log.info("Checking mempool...")
        self.sync_mempools()
        mempool_info = miner.getmempoolinfo()
        assert_equal(mempool_info['size'], len(txids))
        mempool_bytes = mempool_info['bytes']
        self.log.info("Miner's mempool size: %d bytes" % mempool_bytes)
        assert_greater_than_or_equal(mempool_bytes, 1000000)

    def mine_and_checkblock(self, miner, alice):
        self.log.info("Mining the block...")
        bhash, elapsed = timed(lambda: miner.generate(1)[0])
        self.log.info("Block mined in %d seconds" % elapsed)
        _, elapsed = timed(lambda: self.sync_all())
        bsize = alice.getblock(bhash, True)["size"]
        self.log.info("Peers synced in %d seconds. Block size: %d" % (elapsed, bsize))
        # Only shielded txes in mempool. Block size must be below
        # MAX_SHIELDED_BLOCKSIZE + 513 (header + coinbase + coinstake)
        assert_greater_than(MAX_SHIELDED_BLOCKSIZE + 513, bsize)

    def send_shielded(self, node, n_txes, from_address, shield_to):
        txids = []
        for i in range(n_txes):
            txids.append(node.shieldsendmany(from_address, shield_to))
            if (i + 1) % 200 == 0:
                self.log.info("...%d Transactions created..." % (i + 1))
                self.sync_mempools()
        return txids


    def run_test(self):
        miner = self.nodes[0]
        alice = self.nodes[1]
        # First mine 300 blocks
        self.log.info("Generating 300 blocks...")
        miner.generate(300)
        self.sync_blocks()
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'active')

        # -- First check that the miner never produces blocks with more than 750kB of shielded txes

        # Split 10 utxos (of 250 PIV each) in 1000 new utxos of ~2.5 PIV each (to alice)
        UTXOS_TO_SPLIT = 10
        UTXOS_TO_SHIELD = UTXOS_TO_SPLIT * 100
        self.log.info("Creating %d utxos..." % UTXOS_TO_SHIELD)
        txids = self.utxo_splitter(miner, UTXOS_TO_SPLIT, alice)
        assert_equal(len(txids), UTXOS_TO_SPLIT)
        miner.generate(2)
        self.sync_blocks()
        new_utxos = alice.listunspent()
        assert_equal(len(new_utxos), UTXOS_TO_SHIELD)

        # Now alice shields the new utxos individually (fixed 0.2 PIV fee --> ~2.3 PIV notes)
        self.log.info("Shielding utxos...")
        alice_z_addr = alice.getnewshieldaddress()
        shield_to = [{"address": alice_z_addr, "amount": new_utxos[0]["amount"] - Decimal("0.2")}]
        txids = self.send_shielded(alice, UTXOS_TO_SHIELD, "from_transparent", shield_to)

        # Check mempool
        self.check_mempool(miner, txids)

        # Mine the block
        self.mine_and_checkblock(miner, alice)
        self.log.info("Done. %d txes still in mempool." % miner.getmempoolinfo()['size'])


if __name__ == '__main__':
    SaplingFillBlockTest().main()
