#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Tests importprivkey and importaddress with staking keys/addresses.
Node0 generates staking addresses and sends delegations to them.
Node1 imports and rescans. The test checks that cold utxos and staking balance is updated.
"""

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    DecimalAmt,
)

class ImportStakingTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [[]] * self.num_nodes

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests importprivkey and importaddress with staking keys/addresses."
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def run_test(self):
        NUM_OF_DELEGATIONS = 4  # Create 2*NUM_OF_DELEGATIONS staking addresses
        self.log_title()

        # Create cold staking addresses and delegations
        self.log.info("Creating new staking addresses and sending delegations")
        staking_addresses = [self.nodes[0].getnewstakingaddress("label %d" % i)
                             for i in range(2 * NUM_OF_DELEGATIONS)]
        delegations = []
        for i, sa in enumerate(staking_addresses):
            # delegate 10 HMS
            delegations.append(self.nodes[0].delegatestake(sa, 10)['txid'])
            # mine a block and check staking balance
            self.nodes[0].generate(1)
            assert_equal(self.nodes[0].getdelegatedbalance(), DecimalAmt(10 * (i+1)))
            self.sync_blocks()

        # Export keys
        self.log.info("Exporting keys and importing in node 1")
        priv_keys = [self.nodes[0].dumpprivkey(x) for x in staking_addresses]

        # Import keys of addresses 0-(NUM_OF_DELEGATIONS-1) (and rescan)
        assert_equal(self.nodes[1].getcoldstakingbalance(), DecimalAmt(0))
        for i, pk in enumerate(priv_keys[:NUM_OF_DELEGATIONS]):
            self.nodes[1].importprivkey(pk, "label %d" % i, True, True)
            val = self.nodes[1].validateaddress(staking_addresses[i])
            assert_equal(val['ismine'], True)
            assert_equal(val['isstaking'], True)
            assert_equal(val['iswatchonly'], False)
            assert_equal(self.nodes[1].getcoldstakingbalance(), DecimalAmt(10 * (i + 1)))
        self.log.info("Balance of node 1 checks out")
        coldutxos = [x['txid'] for x in self.nodes[1].listcoldutxos()]
        assert_equal(len(coldutxos), NUM_OF_DELEGATIONS)
        assert_equal(len([x for x in coldutxos if x in delegations]), NUM_OF_DELEGATIONS)
        self.log.info("Delegation list of node 1 checks out")

        # Import remaining addresses as watch-only (and rescan again)
        self.log.info("Importing addresses (watch-only)")
        for i, sa in enumerate(staking_addresses[NUM_OF_DELEGATIONS:]):
            self.nodes[1].importaddress(sa, "label %d" % i, True)
            # !TODO: add watch-only support in the core (balance and txes)
            # Currently the only way to check the addressbook without the key here
            # is to verify the label with validateaddress
            val = self.nodes[1].validateaddress(sa)
            assert_equal(val['ismine'], False)
            assert_equal(val['isstaking'], True)
            assert_equal(val['iswatchonly'], True)
            assert_equal(self.nodes[1].getcoldstakingbalance(), DecimalAmt(10 * NUM_OF_DELEGATIONS))
        self.log.info("Balance of node 1 checks out")



if __name__ == '__main__':
    ImportStakingTest().main()
