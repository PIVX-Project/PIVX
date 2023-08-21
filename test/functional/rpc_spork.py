#!/usr/bin/env python3
# Copyright (c) 2019-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from time import sleep

from test_framework.test_framework import HemisTestFramework
from test_framework.util import set_node_times, assert_equal


class hemis_RPCSporkTest(HemisTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[]] * self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')

    def setup_chain(self):
        # Start with clean chain
        self.log.info("Initializing test directory " + self.options.tmpdir)
        self._initialize_chain_clean()
        self.enable_mocktime()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Performs tests on the Spork RPC"
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def run_test(self):
        self.log_title()
        set_node_times(self.nodes, self.mocktime)
        sporkName = "SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT"

        # 0 - check SPORK 8 status from node 1 (must be inactive)
        assert_equal(False, self.is_spork_active(1, sporkName))

        # 1 - activate SPORK 8 with nodes[0]
        assert_equal("success", self.activate_spork(0, sporkName))
        sleep(1)
        # check SPORK 8 status from nodes[1] (must be active)
        assert_equal(True, self.is_spork_active(1, sporkName))

        # 2 - Adjust time to 1 sec in the future and deactivate SPORK 8 with node[0]
        self.mocktime += 1
        set_node_times(self.nodes, self.mocktime)
        assert_equal("success", self.deactivate_spork(0, sporkName))
        sleep(1)
        # check SPORK 8 value from nodes[1] (must be inactive again)
        assert_equal(False, self.is_spork_active(1, sporkName))

        # 3 - Adjust time to 1 sec in the future and set new value (mocktime) for SPORK 8 with node[0]
        self.mocktime += 1
        set_node_times(self.nodes, self.mocktime)
        assert_equal("success", self.set_spork(0, sporkName, self.mocktime))
        sleep(1)
        # check SPORK 8 value from nodes[1] (must be equal to mocktime)
        assert_equal(self.mocktime, self.get_spork(1, sporkName))

        # 4 - Stop nodes and check value again after restart
        self.log.info("Stopping nodes...")
        self.stop_nodes()
        self.log.info("Restarting node 1...")
        self.start_node(1, [])
        assert_equal(self.mocktime, self.get_spork(1, sporkName))
        self.log.info("%s: TEST PASSED" % self.__class__.__name__)



if __name__ == '__main__':
    hemis_RPCSporkTest().main()

