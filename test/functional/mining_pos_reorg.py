#!/usr/bin/env python3
# Copyright (c) 2019-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    connect_nodes_clique,
    disconnect_nodes,
    set_node_times,
    DecimalAmt,
)

class ReorgStakeTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [['-nuparams=PoS:201', '-nuparams=PoS_v2:201', "-whitelist=127.0.0.1"]] * self.num_nodes

    def setup_chain(self):
        self.log.info("Initializing test directory " + self.options.tmpdir)
        self._initialize_chain()
        self.enable_mocktime()

    def setup_network(self):
        # connect all nodes between each other
        self.setup_nodes()
        connect_nodes_clique(self.nodes)
        self.sync_all()

    def log_title(self):
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        description = "Tests reorganisation for PoS blocks."
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, description)

    def disconnect_all(self):
        self.log.info("Disconnecting nodes...")
        for i in range(self.num_nodes):
            for j in range(self.num_nodes):
                if j != i:
                    disconnect_nodes(self.nodes[i], j)
        self.log.info("Nodes disconnected")

    def get_tot_balance(self, nodeid):
        wi = self.nodes[nodeid].getwalletinfo()
        assert_equal(self.nodes[nodeid].getblockcount(), wi['last_processed_block'])
        return wi['balance'] + wi['immature_balance']

    def check_money_supply(self, expected_piv):
        # verify that nodes have the expected HMS supply
        piv_supply = [self.nodes[i].getsupplyinfo(True)['transparentsupply']
                      for i in range(self.num_nodes)]
        assert_equal(piv_supply, [DecimalAmt(expected_piv)] * self.num_nodes)


    def run_test(self):

        def findUtxoInList(txid, vout, utxo_list):
            for x in utxo_list:
                if x["txid"] == txid and x["vout"] == vout:
                    return True, x
            return False, None

        # HMS supply: block rewards
        expected_money_supply = 250.0 * 200
        self.check_money_supply(expected_money_supply)
        initial_time = self.mocktime

        # Check balances
        self.log.info("Checking balances...")
        initial_balance = [self.get_tot_balance(i) for i in range(self.num_nodes)]
        # -- 50 pow blocks each
        assert_equal(initial_balance, [DecimalAmt(250.0 * 50)] * self.num_nodes)
        self.log.info("Balances ok.")

        # Disconnect nodes
        self.disconnect_all()

        # Stake one block with node-0 and save the stake input
        self.log.info("Staking 1 block with node 0...")
        initial_unspent_0 = self.nodes[0].listunspent()
        self.mocktime = self.generate_pos(0, self.mocktime)
        last_block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())
        assert(len(last_block["tx"]) > 1)   # a PoS block has at least two txes
        coinstake_txid = last_block["tx"][1]
        coinstake_tx = self.nodes[0].getrawtransaction(coinstake_txid, True)
        assert (coinstake_tx["vout"][0]["scriptPubKey"]["hex"] == "")  # first output of coinstake is empty
        stakeinput = coinstake_tx["vin"][0]

        # The stake input was unspent 1 block ago, now it's not
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], initial_unspent_0)
        assert (res)
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert (not res)
        self.log.info("Coinstake input %s...%s-%d is no longer spendable." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))

        # Stake 10 more blocks with node-0 and check balances
        self.log.info("Staking 10 more blocks with node 0...")
        for i in range(10):
            self.mocktime = self.generate_pos(0, self.mocktime)
        expected_balance_0 = initial_balance[0] + DecimalAmt(11 * 250.0)
        assert_equal(self.get_tot_balance(0), expected_balance_0)
        self.log.info("Balance for node 0 checks out.")

        # Connect with node 2 and sync
        self.log.info("Reconnecting node 0 and node 2")
        connect_nodes(self.nodes[0], 2)
        self.sync_blocks([self.nodes[i] for i in [0, 2]])

        # verify that the stakeinput can't be spent
        stakeinput_tx_json = self.nodes[0].getrawtransaction(stakeinput["txid"], True)
        stakeinput_amount = float(stakeinput_tx_json["vout"][int(stakeinput["vout"])]["value"])
        rawtx_unsigned = self.nodes[0].createrawtransaction(
            [{"txid": stakeinput["txid"], "vout": int(stakeinput["vout"])}],
            {"xxncEuJK27ygNh7imNfaX8JV6ZQUnoBqzN": (stakeinput_amount-0.01)})
        rawtx = self.nodes[0].signrawtransaction(rawtx_unsigned)
        assert(rawtx["complete"])
        assert_raises_rpc_error(-25, "Missing inputs", self.nodes[0].sendrawtransaction, rawtx["hex"])
        txid = self.nodes[0].decoderawtransaction(rawtx["hex"])["txid"]
        assert_raises_rpc_error(-5, "No such mempool or blockchain transaction",
                                self.nodes[0].getrawtransaction, txid)
        self.log.info("GOOD: spending the stake input was not possible.")

        # Stake 12 blocks with node-1
        self.mocktime = initial_time
        set_node_times(self.nodes, self.mocktime)
        self.log.info("Staking 12 blocks with node 1...")
        for i in range(12):
            self.mocktime = self.generate_pos(1, self.mocktime)
        expected_balance_1 = initial_balance[1] + DecimalAmt(12 * 250.0)
        assert_equal(self.get_tot_balance(1), expected_balance_1)
        self.log.info("Balance for node 1 checks out.")

        # re-connect and sync nodes and check that node-0 and node-2 get on the other chain
        new_best_hash = self.nodes[1].getbestblockhash()
        self.log.info("Connecting and syncing nodes...")
        set_node_times(self.nodes, self.mocktime)
        connect_nodes_clique(self.nodes)
        self.sync_blocks()
        for i in [0, 2]:
            assert_equal(self.nodes[i].getbestblockhash(), new_best_hash)

        # check balance of node-0
        assert_equal(self.get_tot_balance(0), initial_balance[0])
        self.log.info("Balance for node 0 checks out.")

        # check that NOW the original stakeinput is present and spendable
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert res and utxo["spendable"]
        self.log.info("Coinstake input %s...%s-%d is spendable again." % (
            stakeinput["txid"][:9], stakeinput["txid"][-4:], stakeinput["vout"]))
        self.nodes[0].sendrawtransaction(rawtx["hex"])
        self.sync_mempools()
        self.mocktime = self.generate_pos(1, self.mocktime)
        self.sync_blocks()
        res, utxo = findUtxoInList(stakeinput["txid"], stakeinput["vout"], self.nodes[0].listunspent())
        assert not res

        # Verify that HMS supply was properly updated after the reorgs (including burned fee)
        self.log.info("Check HMS supply...")
        expected_money_supply += 250.0 * (self.nodes[1].getblockcount() - 200) - 0.01
        self.check_money_supply(expected_money_supply)
        self.log.info("Supply checks out.")


if __name__ == '__main__':
    ReorgStakeTest().main()
