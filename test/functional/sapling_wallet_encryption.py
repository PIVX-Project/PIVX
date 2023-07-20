#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    assert_true
)

# Test encrypted wallet behaviour with Sapling addresses
class WalletSaplingTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]
        node.generate(1)

        # wallet should currently be empty
        assert_equal(len(node.listshieldaddresses()), 0)
        # create 100 keys
        keys_to_generate = 100
        for _ in range (keys_to_generate):
            node.getnewshieldaddress()
        # Verify we can list the created addresses
        addr_list = node.listshieldaddresses()
        assert_equal(len(addr_list), keys_to_generate)

        password = "hello"
        node.encryptwallet(password)
        # assert that the wallet is encrypted
        assert_raises_rpc_error(-15, "Error: running with an encrypted wallet, but encryptwallet was called.", node.encryptwallet, 'ff')

        # now verify addresses again
        addr_list_post_encryption = node.listshieldaddresses()
        assert_equal(len(addr_list_post_encryption), keys_to_generate)
        assert_equal(addr_list, addr_list_post_encryption)

        # try to add a new key, but we can't as the wallet is locked
        assert_raises_rpc_error(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.", node.getnewshieldaddress)

        # now unlock the wallet and try to generate the key again
        node.walletpassphrase(password, 12000)
        new_addr = node.getnewshieldaddress()
        assert(new_addr is not None)

        # and verify that the key has been added
        found = False
        addr_list_post_encryption = node.listshieldaddresses()
        assert_equal(len(addr_list_post_encryption), keys_to_generate + 1)
        for addr in addr_list_post_encryption:
            if addr == new_addr:
                found = True
        assert_true(found, "error: shield address not found in encrypted wallet")

if __name__ == '__main__':
    WalletSaplingTest().main()
