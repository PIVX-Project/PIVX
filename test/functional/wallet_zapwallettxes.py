#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the zapwallettxes functionality.

- start two hemisd nodes
- create two transactions on node 0 - one is confirmed and one is unconfirmed.
- restart node 0 and verify that both the confirmed and the unconfirmed
  transactions are still available.
- restart node 0 with zapwallettxes and persistmempool, and verify that both
  the confirmed and the unconfirmed transactions are still available.
- restart node 0 with just zapwallettxes and verify that the confirmed
  transactions are still available, but that the unconfirmed transaction has
  been zapped.
"""

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

class ZapWalletTXesTest (PivxTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # whitelist all peers to speed up tx relay / mempool sync
        self.extra_args = [["-whitelist=127.0.0.1"]] * self.num_nodes

    def run_test(self):
        self.log.info("Mining blocks...")
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 250)
        # This transaction will be confirmed
        txid1 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 10)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        balance_nodes = [self.nodes[i].getbalance() for i in range(self.num_nodes)]

        # This transaction will not be confirmed
        txid2 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 20)
        self.sync_mempools(wait=.1)

        # Confirmed and unconfirmed transactions are now in the wallet.
        assert_equal(self.nodes[0].gettransaction(txid1)['txid'], txid1)
        assert_equal(self.nodes[0].gettransaction(txid2)['txid'], txid2)

        # Exercise balance rpcs
        assert_equal(self.nodes[1].getwalletinfo()["unconfirmed_balance"], 20)
        assert_equal(self.nodes[1].getunconfirmedbalance(), 20)

        # Restart node0. Both confirmed and unconfirmed transactions remain in the wallet.
        self.restart_node(0)

        assert_equal(self.nodes[0].gettransaction(txid1)['txid'], txid1)
        assert_equal(self.nodes[0].gettransaction(txid2)['txid'], txid2)

        # Stop nodes and restart with zapwallettxes and persistmempool. The unconfirmed
        # transaction is zapped from the wallet, but is re-added when the mempool is reloaded.
        # original balances are restored
        for i in range(1, 3):
            self.log.info("Restarting with --zapwallettxes=%d" % i)
            self.stop_nodes()
            self.start_node(0, ["-zapwallettxes=%d" % i])
            self.start_node(1, ["-zapwallettxes=%d" % i])

            # tx1 is still be available because it was confirmed
            assert_equal(self.nodes[0].gettransaction(txid1)['txid'], txid1)

            # This will raise an exception because the unconfirmed transaction has been zapped
            assert_raises_rpc_error(-5, 'Invalid or non-wallet transaction id', self.nodes[0].gettransaction, txid2)

            # Check (confirmed) balances
            assert_equal(balance_nodes, [self.nodes[i].getbalance() for i in range(self.num_nodes)])

if __name__ == '__main__':
    ZapWalletTXesTest().main()
