#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the -alertnotify, -blocknotify and -walletnotify options."""

import os

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    wait_until
)


class NotificationsTest(PivxTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.alertnotify_dir = os.path.join(self.options.tmpdir, "alertnotify")
        self.blocknotify_dir = os.path.join(self.options.tmpdir, "blocknotify")
        self.walletnotify_dir = os.path.join(self.options.tmpdir, "walletnotify")
        os.mkdir(self.alertnotify_dir)
        os.mkdir(self.blocknotify_dir)
        os.mkdir(self.walletnotify_dir)

        # -alertnotify and -blocknotify on node0, walletnotify on node1
        self.extra_args = [["-blockversion=8",
                            "-alertnotify=echo > {}".format(os.path.join(self.alertnotify_dir, '%s')),
                            "-blocknotify=echo > {}".format(os.path.join(self.blocknotify_dir, '%s'))],
                           ["-blockversion=211",
                            "-rescan",
                            "-walletnotify=echo > {}".format(os.path.join(self.walletnotify_dir, '%s'))]]
        super().setup_network()

    # Hardcoded, always true, function for now to make future back ports easier.
    def is_wallet_compiled(self):
        return True

    def run_test(self):
        self.log.info("test -blocknotify")
        block_count = 10
        blocks = self.nodes[1].generate(block_count)

        # wait at most 10 seconds for expected number of files before reading the content
        wait_until(lambda: len(os.listdir(self.blocknotify_dir)) == block_count, timeout=10)

        # directory content should equal the generated blocks hashes
        assert_equal(sorted(blocks), sorted(os.listdir(self.blocknotify_dir)))

        if self.is_wallet_compiled():
            self.log.info("test -walletnotify")
            # wait at most 10 seconds for expected number of files before reading the content
            wait_until(lambda: len(os.listdir(self.walletnotify_dir)) == block_count, timeout=10)

            # directory content should equal the generated transaction hashes
            txids_rpc = list(map(lambda t: t['txid'], self.nodes[1].listtransactions("*", block_count)))
            assert_equal(sorted(txids_rpc), sorted(os.listdir(self.walletnotify_dir)))
            self.stop_node(1)
            for tx_file in os.listdir(self.walletnotify_dir):
                os.remove(os.path.join(self.walletnotify_dir, tx_file))

            self.log.info("test -walletnotify after rescan")
            # restart node to rescan to force wallet notifications
            self.start_node(1)
            self.connect_nodes(0, 1)

            wait_until(lambda: len(os.listdir(self.walletnotify_dir)) == block_count, timeout=10)

            # directory content should equal the generated transaction hashes
            txids_rpc = list(map(lambda t: t['txid'], self.nodes[1].listtransactions("*", block_count)))
            assert_equal(sorted(txids_rpc), sorted(os.listdir(self.walletnotify_dir)))

        # Mine another 41 up-version blocks. -alertnotify should trigger on the 51st.
        self.log.info("test -alertnotify")
        self.nodes[1].generate(51)
        self.sync_all()

        # Give pivxd 10 seconds to write the alert notification
        wait_until(lambda: len(os.listdir(self.alertnotify_dir)), timeout=10)

        for notify_file in os.listdir(self.alertnotify_dir):
            os.remove(os.path.join(self.alertnotify_dir, notify_file))

        # Mine more up-version blocks, should not get more alerts:
        self.nodes[1].generate(2)
        self.sync_all()

        self.log.info("-alertnotify should not continue notifying for more unknown version blocks")
        assert_equal(len(os.listdir(self.alertnotify_dir)), 0)

if __name__ == '__main__':
    NotificationsTest().main()
