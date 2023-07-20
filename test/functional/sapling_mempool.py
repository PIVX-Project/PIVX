#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


# Test mempool interaction with Sapling transactions
class SaplingMempoolTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-nuparams=v5_shield:1']] * self.num_nodes

    def run_test(self):
        miner = self.nodes[0]
        alice = self.nodes[1]

        # Fixed fee
        fee = 0.05

        self.log.info("Mining 120 blocks...")
        miner.generate(120)
        self.sync_all()
        # Sanity-check the test harness
        assert_equal([x.getblockcount() for x in self.nodes], [120] * self.num_nodes)

        # miner sends a 10 PIV note to Alice
        self.log.info("Shielding some coins for Alice...")
        alice_zaddr = alice.getnewshieldaddress()
        miner.shieldsendmany("from_transparent", [{"address": alice_zaddr, "amount": Decimal('10.00')}], 1, fee)
        miner.generate(1)
        self.sync_all()
        assert_equal(alice.getshieldbalance(alice_zaddr), Decimal('10.00'))

        # Alice creates (but doesn't send) tx_A to transparent address tadd_A
        self.log.info("Alice creating tx_A...")
        tadd_A = alice.getnewaddress()
        rawTx_hex = alice.rawshieldsendmany(alice_zaddr, [{"address": tadd_A, "amount": Decimal('9.00')}], 1, fee)

        # Alice creates and sends tx_B, unshielding the same note to tadd_B
        self.log.info("Alice creating and sending tx_B...")
        tadd_B = alice.getnewaddress()
        txid_B = alice.shieldsendmany(alice_zaddr, [{"address": tadd_B, "amount": Decimal('9.95')}], 1, fee)

        # Miner receives tx_B and accepts it in the mempool
        assert (txid_B in alice.getrawmempool())
        self.sync_mempools()
        assert(txid_B in miner.getrawmempool())
        self.log.info("tx_B accepted in the memory pool.")

        # Now tx_A would double-spend the sapling note in the memory pool
        assert_raises_rpc_error(-26, "bad-txns-nullifier-double-spent",
                                alice.sendrawtransaction, rawTx_hex)
        self.log.info("tx_A NOT accepted in the mempool. Good.")

        # Mine tx_B and try to send tx_A again
        self.log.info("Mine a block and verify that tx_B gets on chain")
        miner.generate(1)
        self.sync_all()
        txB_json = alice.getrawtransaction(txid_B, True)
        assert("blockhash" in txB_json)
        self.log.info("trying to relay tx_A again...")
        assert_raises_rpc_error(-26, "bad-txns-shielded-requirements-not-met",
                                alice.sendrawtransaction, rawTx_hex)
        self.log.info("tx_A NOT accepted in the mempool. Good.")

        # miner sends another 10 PIV note to Alice
        self.log.info("Shielding some more coins for Alice...")
        miner.shieldsendmany("from_transparent", [{"address": alice_zaddr, "amount": Decimal('10.00')}], 1, fee)
        miner.generate(1)
        self.sync_all()
        assert_equal(alice.getshieldbalance(alice_zaddr), Decimal('10.00'))

        # Alice creates and sends tx_C, unshielding the note to tadd_C
        self.log.info("Alice creating and sending tx_C...")
        tadd_C = alice.getnewaddress()
        txC_hex = alice.rawshieldsendmany(alice_zaddr, [{"address": tadd_C, "amount": Decimal('9.00')}], 1, fee)
        txid_C = alice.sendrawtransaction(txC_hex)

        # Miner receives tx_C and accepts it in the mempool
        self.sync_mempools()
        assert(txid_C in miner.getrawmempool())
        self.log.info("tx_C accepted in the memory pool.")

        # Now disconnect the block with the note's anchor,
        # and check that the tx is removed from the mempool
        self.log.info("Disconnect the last block to change the sapling anchor")
        anchor = alice.decoderawtransaction(txC_hex)['vShieldSpend'][0]['anchor']
        assert_equal(anchor, miner.getbestsaplinganchor())
        miner.invalidateblock(miner.getbestblockhash())
        assert (anchor != miner.getbestsaplinganchor())
        assert(txid_C not in miner.getrawmempool())
        self.log.info("Good. tx_C removed from the memory pool.")


if __name__ == '__main__':
    SaplingMempoolTest().main()
