#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the blocksdir option."""

from test_framework.test_framework import PivxTestFramework, initialize_datadir

import os
import shutil

class BlocksdirTest(PivxTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        self.stop_node(0)
        assert os.path.isdir(os.path.join(self.nodes[0].datadir, "regtest", "blocks"))
        assert not os.path.isdir(os.path.join(self.nodes[0].datadir, "blocks"))
        shutil.rmtree(self.nodes[0].datadir)
        initialize_datadir(self.options.tmpdir, 0)
        self.log.info("Starting with nonexistent blocksdir ...")
        self.assert_start_raises_init_error(0, ["-blocksdir="+self.options.tmpdir+ "/blocksdir"], "Specified blocks director")
        os.mkdir(self.options.tmpdir+ "/blocksdir")
        self.log.info("Starting with existing blocksdir ...")
        self.start_node(0, ["-blocksdir="+self.options.tmpdir+ "/blocksdir"])
        self.log.info("mining blocks..")
        self.nodes[0].generate(10)
        assert(os.path.isfile(os.path.join(self.options.tmpdir, "blocksdir", "regtest", "blocks", "blk00000.dat")))
        assert(os.path.isdir(os.path.join(self.options.tmpdir, "node0", "regtest", "blocks", "index")))

if __name__ == '__main__':
    BlocksdirTest().main()
