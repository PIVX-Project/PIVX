#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPCs related to blockchainstate.

Test the following RPCs:
    - getblockchaininfo
    - gettxoutsetinfo
    - getdifficulty
    - getbestblockhash
    - getblockhash
    - getblockheader
    - getchaintxstats
    - getnetworkhashps
    - verifychain

Tests correspond to code in rpc/blockchain.cpp.
"""

from decimal import Decimal

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than_or_equal,
    assert_raises_rpc_error,
    assert_is_hex_string,
    assert_is_hash_string,
)

class BlockchainTest(HemisTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        self._test_getblockchaininfo()
        self._test_gettxoutsetinfo()
        self._test_getblockheader()
        self._test_getblock()
        #self._test_getdifficulty()
        self.nodes[0].verifychain(0)

    def _test_getblockchaininfo(self):
        self.log.info("Test getblockchaininfo")

        keys = [
            'bestblockhash',
            'blocks',
            'chain',
            'chainwork',
            'difficulty',
            'headers',
            'initial_block_downloading',
            'shield_pool_value',
            'softforks',
            'upgrades',
            'verificationprogress',
            'warnings',
        ]
        res = self.nodes[0].getblockchaininfo()
        # result should have these additional pruning keys if manual pruning is enabled
        assert_equal(sorted(res.keys()), sorted(keys))

    def _test_gettxoutsetinfo(self):
        node = self.nodes[0]
        res = node.gettxoutsetinfo()

        assert_equal(res['total_amount'], Decimal('50000.00000000'))
        assert_equal(res['transactions'], 200)
        assert_equal(res['height'], 200)
        assert_equal(res['txouts'], 200)
        assert_equal(res['bestblock'], node.getblockhash(200))
        size = res['disk_size']
        assert_greater_than_or_equal(size, 6400)
        assert_greater_than_or_equal(64000, size)
        assert_equal(len(res['bestblock']), 64)
        assert_equal(len(res['hash_serialized_2']), 64)

    def _test_getblockheader(self):
        node = self.nodes[0]

        assert_raises_rpc_error(-8, "blockhash must be of length 64 (not 8, for 'nonsense')",
                                node.getblockheader, "nonsense")

        besthash = node.getbestblockhash()
        secondbesthash = node.getblockhash(199)
        header = node.getblockheader(besthash)

        assert_equal(header['hash'], besthash)
        assert_equal(header['height'], 200)
        assert_equal(header['confirmations'], 1)
        assert_equal(header['previousblockhash'], secondbesthash)
        assert_is_hex_string(header['chainwork'])
        assert_is_hash_string(header['hash'])
        assert_is_hash_string(header['previousblockhash'])
        assert_is_hash_string(header['merkleroot'])
        assert_is_hash_string(header['bits'], length=None)
        assert isinstance(header['time'], int)
        #assert isinstance(header['mediantime'], int)
        assert isinstance(header['nonce'], int)
        assert isinstance(header['version'], int)
        #assert isinstance(int(header['versionHex'], 16), int)
        assert isinstance(header['difficulty'], Decimal)

    def _test_getdifficulty(self):
        difficulty = self.nodes[0].getdifficulty()
        # 1 hash in 2 should be valid, so difficulty should be 1/2**31
        # binary => decimal => binary math is why we do this check
        assert abs(difficulty * 2**31 - 1) < 0.0001

    def _test_getblock(self):
        node = self.nodes[0]

        # Test getblock verbosity
        besthash = node.getbestblockhash()
        assert_is_hex_string(node.getblock(blockhash=besthash, verbose=False))
        assert_is_hex_string(node.getblock(blockhash=besthash, verbosity=0))

        assert_is_hash_string(node.getblock(besthash, 1)['tx'][0])
        assert_is_hash_string(node.getblock(besthash, True)['tx'][0])
        assert_is_hex_string(node.getblock(besthash, 2)['tx'][0]['vin'][0]['coinbase'])


if __name__ == '__main__':
    BlockchainTest().main()
