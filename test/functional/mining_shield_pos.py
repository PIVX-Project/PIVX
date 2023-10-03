#!/usr/bin/env python3
# Copyright (c) 2023 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import PivxTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    disconnect_nodes,
)
import time


class PIVX_ShieldStakingTest(PivxTestFramework):

    def generate_shield_pos(self, node_id):
        assert_greater_than(len(self.nodes), node_id)
        rpc_conn = self.nodes[node_id]
        ss = rpc_conn.getstakingstatus()
        assert ss["walletunlocked"]
        assert ss["shield_stakeables_notes"] > 0
        assert ss["shield_staking_balance"] > 0.0
        fStaked = False
        failures = 0
        while not fStaked:
            try:
                res = rpc_conn.generate(1)
                fStaked = True
                return res
            except JSONRPCException as e:
                if ("Couldn't create new block" in str(e)):
                    failures += 1
                    # couldn't generate block. check that this node can still stake (after 60 failures)
                    if failures > 60:
                        ss = rpc_conn.getstakingstatus()
                        if not (ss["walletunlocked"] and ss["shield_stakeables_notes"] > 0 and ss["shield_staking_balance"] > 0.0):
                            raise AssertionError("Node %d unable to stake!" % node_id)
                    # try to stake one sec in the future
                    time.sleep(1)
                else:
                    raise e

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def run_test(self):
        self.log.info("Shield stake functional tests")

        # Mine enough blocks to activate shield staking" + a bonus so node0 has enough pivs to mine the final block
        self.nodes[0].generate(800)
        self.sync_all()

        # Send some shielded notes to node1
        saplingAddr1 = self.nodes[1].getnewshieldaddress()
        txid = self.nodes[0].sendtoaddress(saplingAddr1, 40000, "", "", True)

        # Generate more blocks so the shield notes becomes stakeable
        self.nodes[0].generate(20)
        self.sync_all()

        # Sanity check, shield staking is activated
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'active')
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v6 evo']['status'], 'active')
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['shield staking']['status'], 'active')

        # Node1 has exactly one shield stakeable note
        assert_equal(self.nodes[1].getstakingstatus()['shield_stakeables_notes'], 1)
        assert_equal(self.nodes[1].getstakingstatus()['transparent_stakeable_coins'], 0)

        # Before generating the shield stake block isolate node 2:
        disconnect_nodes(self.nodes[0], 2)
        disconnect_nodes(self.nodes[2], 0)
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)

        # Generate the block and check that reward is 4 shield PIVs
        initialBalance = self.nodes[1].getshieldbalance()
        shieldStakeBlockHash = self.generate_shield_pos(1)[0]
        assert_equal(self.nodes[1].getshieldbalance()-initialBalance, 4)

        # Check that a loose coinShieldStakeTx cannot go in mempool
        coinShieldStakeTxHash = self.nodes[1].getblock(shieldStakeBlockHash)['tx'][1]
        coinShieldStakeTxHex = self.nodes[1].gettransaction(coinShieldStakeTxHash)['hex']
        assert_raises_rpc_error(-26, "coinshieldstake", self.nodes[2].sendrawtransaction, coinShieldStakeTxHex)

        # Check that node0 accepted the block:
        self.sync_all(self.nodes[0:2])
        assert_equal(self.nodes[1].getblockhash(821), shieldStakeBlockHash)
        assert_equal(self.nodes[0].getblockhash(821), shieldStakeBlockHash)

        # Finally verify that the reward can be spent:
        recipient = [{"address": self.nodes[0].getnewshieldaddress(), "amount": (initialBalance+2)}]
        txid = self.nodes[1].shieldsendmany("from_shield", recipient)
        self.sync_all(self.nodes[0:2])
        blockHash = self.nodes[0].generate(1)[0]
        self.sync_all(self.nodes[0:2])
        assert (txid in self.nodes[0].getblock(blockHash)['tx'])
        assert_equal(self.nodes[1].getblockhash(822), blockHash)


if __name__ == '__main__':
    PIVX_ShieldStakingTest().main()
