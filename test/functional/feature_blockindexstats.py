#!/usr/bin/env python3
# Copyright (c) 2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
""" Test getblockindexstats RPC results"""

import random

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
)


class BlockIndexStatsTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        saplingUpgrade = ['-nuparams=v5_shield:201']
        self.extra_args = [saplingUpgrade] * self.num_nodes

    def send_tx(self, node_from, node_to, fee, fFromShield, fToShield):
        if not fFromShield and not fToShield:
            # t->t (between 1 and 3 inputs)
            spendable = sorted(node_from.listunspent(), key=lambda x: x["amount"])
            outVal = 0
            inputs = []
            for _ in range(random.randint(1, 3)):
                if len(spendable) == 0:
                    raise Exception("Not enough inputs")
                utxo = spendable.pop()
                inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
                outVal += float(utxo["amount"])
            # add 1 HMS "change" output 50% of the times
            if random.randint(1, 2) == 1 and outVal > 1.0 + fee:
                outputs = {node_to.getnewaddress(): round(outVal - 1.0 - fee, 8),
                           node_from.getnewaddress(): 1.0}
            else:
                outputs = {node_to.getnewaddress(): round(outVal - fee, 8)}
            hex_tx = node_from.signrawtransaction(node_from.createrawtransaction(inputs, outputs))["hex"]
        else:
            # shield txes (between 1 and 3 inputs)
            from_input = node_from.listshieldunspent() if fFromShield else node_from.listunspent()
            spendable = sorted(from_input, key=lambda x: x["amount"])
            outVal = 0.0
            for _ in range(random.randint(1, 3)):
                if len(spendable) == 0:
                    raise Exception("Not enough inputs")
                outVal += float(spendable.pop()["amount"])
            dest = node_to.getnewshieldaddress() if fToShield else node_to.getnewaddress()
            from_str = "from_shield" if fFromShield else "from_transparent"
            # add 1 HMS "change" output 50% of the times
            if random.randint(1, 2) == 1 and outVal > 1.0 + fee:
                change_dest = node_from.getnewshieldaddress() if fFromShield else node_from.getnewaddress()
                outputs = [{"address": dest, "amount": round(outVal - 1.0 - fee, 8)},
                           {"address": change_dest, "amount": 1.0}]
            else:
                outputs = [{"address": dest, "amount": round(outVal - fee, 8)}]
            hex_tx = node_from.rawshieldsendmany(from_str, outputs, 1, fee)
        # Return txid, size
        return node_from.sendrawtransaction(hex_tx), node_from.decoderawtransaction(hex_tx)["size"]

    def send_t_t(self, node_from, node_to, fee):
        return self.send_tx(node_from, node_to, fee, False, False)

    def send_t_z(self, node_from, node_to, fee):
        return self.send_tx(node_from, node_to, fee, False, True)

    def send_z_t(self, node_from, node_to, fee):
        return self.send_tx(node_from, node_to, fee, True, False)

    def send_z_z(self, node_from, node_to, fee):
        return self.send_tx(node_from, node_to, fee, True, True)


    def run_test(self):
        miner = self.nodes[0]
        alice = self.nodes[1]
        miner.generate(1)
        self.sync_all()

        # Mine 2 blocks. For each block, the miner creates 30 shield notes.
        # miner creates 20 notes of 10 HMS + 9 notes of 5 HMS + 1 note of 4 HMS (using 1.0 fee)
        self.log.info("Creating 60 Shield notes...")
        for _ in range(2):
            outputs = []
            for i in range(29):
                amt = 10.0 if i < 20 else 5.0
                outputs.append({"address": miner.getnewshieldaddress(), "amount": amt})
            outputs.append({"address": miner.getnewshieldaddress(), "amount": 4.0})
            miner.shieldsendmany("from_transparent", outputs, 1, 1.0)
            miner.generate(1)
        self.log.info("Notes created.")

        miner.generate(3)
        self.sync_all()
        start_block = 206
        assert_equal(alice.getblockcount(), start_block)

        count_tx = 0
        count_bytes = 0
        count_fees = 0.0

        # Mine 30 blocks. Send a tx (either t->t, z->t, t->z, or z->z) each block, with random fee.
        NUM_BLOCKS = 30
        for _ in range(NUM_BLOCKS):
            # 1...4=(t->t) 5=(t->z) 6=(z->t) 7=(z->z)
            tx_kind = random.randint(1, 7)
            fee = round(0.0001 * random.randint(3, 50), 8)
            if tx_kind < 5:
                # transparent tx
                self.log.info("Sending t->t with fee %.8f" % fee)
                txid, txsize = self.send_t_t(miner, alice, fee)
            else:
                # shield tx (adjust fee 100x)
                fee = round(fee * 100, 8)
                if tx_kind == 5:
                    self.log.info("Sending t->z with fee %.8f" % fee)
                    txid, txsize = self.send_t_z(miner, alice, fee)
                elif tx_kind == 6:
                    self.log.info("Sending z->t with fee %.8f" % fee)
                    txid, txsize = self.send_z_t(miner, alice, fee)
                else:
                    self.log.info("Sending z->z with fee %.8f" % fee)
                    txid, txsize = self.send_z_z(miner, alice, fee)
            miner.generate(1)
            self.sync_all()
            count_tx += 1
            count_bytes += txsize
            count_fees += fee

        count_fees = round(count_fees, 8)
        feePerK = round(1000 * count_fees / count_bytes, 8)
        self.log.info("Total txes: %d, bytes: %d, fees: %.8f, feePerK: %.8f" % (
            count_tx, count_bytes, count_fees, feePerK))

        alice_stats = alice.getblockindexstats(start_block+1, NUM_BLOCKS)
        assert_equal(count_tx, alice_stats['txcount'])
        assert_equal(count_tx + NUM_BLOCKS, alice_stats['txcount_all'])
        assert_equal(count_bytes, alice_stats['txbytes'])
        assert_equal(count_fees, float(alice_stats['ttlfee']))



if __name__ == '__main__':
    BlockIndexStatsTest().main()
