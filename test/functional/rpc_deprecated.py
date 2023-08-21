#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test deprecation of RPC calls."""

from test_framework.test_framework import HemisTestFramework
# from test_framework.util import assert_raises_rpc_error


class DeprecatedRpcTest(HemisTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # self.extra_args = [[], ["-deprecatedrpc=rpcname"]] add deprecated rpc here

    def run_test(self):
        # This test should be used to verify correct behaviour of deprecated
        # RPC methods with and without the -deprecatedrpc flags. For example:
        #
        # self.log.info("Make sure that -deprecatedrpc=accounts allows it to take accounts")
        # assert_raises_rpc_error(-32, "listaccounts is deprecated", self.nodes[0].listaccounts)
        # self.nodes[1].listaccounts()
        self.log.info("No test cases to run")  # remove this when adding any tests to this file


if __name__ == '__main__':
    DeprecatedRpcTest().main()
