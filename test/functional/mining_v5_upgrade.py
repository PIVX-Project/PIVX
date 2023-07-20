#!/usr/bin/env python3
# Copyright (c) 2012-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Simple test checking chain movement after v5 enforcement."""

from test_framework.test_framework import PivxTestFramework
from test_framework.util import assert_equal


class MiningV5UpgradeTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]
        self.setup_clean_chain = True

    def run_test(self):
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'pending')
        self.nodes[0].generate(300) # v5 activation height
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'active')
        self.nodes[0].generate(25) # 25 more to check chain movement
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'active')
        assert_equal(self.nodes[0].getblockcount(), 325)


if __name__ == '__main__':
    MiningV5UpgradeTest().main()
