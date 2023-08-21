#!/usr/bin/env python3
# Copyright (c) 2019-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import random
from time import sleep

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
)


class BlockHashCacheTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # This test can go up to block 400 (2 * CACHE_SIZE). Delay POS activation
        self.extra_args = [['-nuparams=PoS:401', '-nuparams=hemis_v3.4:402']]

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests the block-hashes rolling cache."
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def run_test(self):
        self.log_title()
        self.node = self.nodes[0]
        CACHE_SIZE = 200
        # Start with the genesis block
        vBlocks = [self.node.getbestblockhash()]

        # Mine blocks and save the hashes
        self.log.info("Mining %d blocks..." % (CACHE_SIZE - 1))
        for i in range(CACHE_SIZE-1):
            vBlocks.append(self.node.generate(1)[0])

        # Check the cache
        cache = self.node.getcachedblockhashes()
        assert_equal(vBlocks, cache)
        self.log.info("First %d blocks check out" % (CACHE_SIZE - 1))

        # Mine a block and check the cache
        self.log.info("Mining another block...")
        vBlocks.append(self.node.generate(1)[0])
        height = self.node.getblockcount()
        assert_equal(height, CACHE_SIZE)
        assert_equal(len(vBlocks), height + 1)      # genesis included
        cache = self.node.getcachedblockhashes()
        assert_equal(vBlocks[CACHE_SIZE], cache[0])
        self.log.info("Block %d correctly cached (overwriting genesis hash)" % CACHE_SIZE)

        # Mine a random number of blocks between 1 and CACHE_SIZE-1
        x = random.randint(1, CACHE_SIZE - 1)
        self.log.info("Mining %d more blocks..." % x)
        for i in range(x):
            vBlocks.append(self.node.generate(1)[0])

        # Check that the cache is (partially) overwritten
        cache = self.node.getcachedblockhashes()
        for i in range(1, x+1):
            assert_equal(vBlocks[CACHE_SIZE + i], cache[i])
        # ...but not completely
        for i in range(x+1, CACHE_SIZE):
            assert_equal(vBlocks[i], cache[i])
        self.log.info("Cached correctly")

        # Disconnect one block
        self.log.info("Disconnecting one block...")
        height = self.node.getblockcount()
        old_hash = self.node.getbestblockhash()
        self.node.invalidateblock(old_hash)
        assert_equal(self.node.getblockcount(), height - 1)
        vBlocks.pop()
        assert_equal(len(vBlocks), height)  # genesis included

        # Check that the cache cycles back
        cache = self.node.getcachedblockhashes()
        assert_equal(vBlocks[height - CACHE_SIZE], cache[height % CACHE_SIZE])
        self.log.info("Disconnecting %d, block at height %d cycles back in" % (
            height, height - CACHE_SIZE))

        # Reconsider the block, mine another on top, and check the cache again
        self.log.info("Reconsidering and mining one more block...")
        self.node.reconsiderblock(old_hash)
        assert_equal(self.node.getblockcount(), height)
        vBlocks.append(old_hash)
        vBlocks.append(self.node.generate(1)[0])
        assert_equal(self.node.getblockcount(), height + 1)
        assert_equal(len(vBlocks), height + 2)  # genesis included
        assert_equal(vBlocks[height], old_hash)
        cache = self.node.getcachedblockhashes()
        assert_equal(vBlocks[height], cache[height % CACHE_SIZE])
        assert_equal(vBlocks[height + 1], cache[(height + 1) % CACHE_SIZE])
        self.log.info("Cached correctly")

        # Restart the node and check that the cache is correctly initialized
        self.log.info("Restarting the node...")
        self.stop_nodes()
        sleep(5)
        self.start_nodes()
        assert_equal(self.node.getcachedblockhashes(), cache)
        self.log.info("All good.")


if __name__ == '__main__':
    BlockHashCacheTest().main()
