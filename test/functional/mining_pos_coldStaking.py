#!/usr/bin/env python3
# Copyright (c) 2019-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
from io import BytesIO
from time import sleep

from test_framework.messages import CTransaction, CTxIn, CTxOut, COIN, COutPoint
from test_framework.script import CScript, OP_CHECKSIG
from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    set_node_times,
)

# filter utxos based on first 5 bytes of scriptPubKey
def getDelegatedUtxos(utxos):
    return [x for x in utxos if x["scriptPubKey"][:10] == '76a97b63d1' or x["scriptPubKey"][:10] == '76a97b63d2']


class hemis_ColdStakingTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        # whitelist all peers to speed up tx relay / mempool sync
        self.extra_args = [['-nuparams=v5_shield:201', "-whitelist=127.0.0.1"]] * self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')
        self.enable_mocktime()

    def setColdStakingEnforcement(self, fEnable=True):
        sporkName = "SPORK_19_COLDSTAKING_MAINTENANCE"
        # update spork 19 with node[0]
        if fEnable:
            self.log.info("Enabling cold staking with SPORK 19...")
            res = self.deactivate_spork(0, sporkName)
        else:
            self.log.info("Disabling cold staking with SPORK 19...")
            res = self.activate_spork(0, sporkName)
        assert_equal(res, "success")
        sleep(1)
        # check that node[1] receives it
        assert_equal(fEnable, not self.is_spork_active(1, sporkName))
        self.log.info("done")

    def isColdStakingEnforced(self):
        # verify from node[1]
        return not self.is_spork_active(1, "SPORK_19_COLDSTAKING_MAINTENANCE")

    def run_test(self):
        self.description = "Performs tests on the Cold Staking P2CS implementation"
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)
        self.DEFAULT_FEE = 0.05
        NUM_OF_INPUTS = 20
        INPUT_VALUE = 249

        # nodes[0] - coin-owner
        # nodes[1] - cold-staker

        # First put cold-staking in maintenance mode
        self.setColdStakingEnforcement(False)
        # double check
        assert (not self.isColdStakingEnforced())

        # 1) nodes[0] and nodes[2] mine 25 blocks each
        # --------------------------------------------
        print("*** 1 ***")
        self.log.info("Mining 50 Blocks...")
        for peer in [0, 2]:
            for j in range(25):
                self.mocktime = self.generate_pow(peer, self.mocktime)
            self.sync_blocks()

        # 2) node[1] sends his entire balance (50 mature rewards) to node[2]
        #  - node[2] stakes a block - node[1] locks the change
        #  - node[0] shields 250 coins (to be delegated later)
        print("*** 2 ***")
        self.log.info("Emptying node1 balance")
        assert_equal(self.nodes[1].getbalance(), 50 * 250)
        txid = self.nodes[1].sendtoaddress(self.nodes[2].getnewaddress(), (50 * 250 - 0.01))
        assert (txid is not None)
        self.sync_mempools()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        # lock the change output (so it's not used as stake input in generate_pos)
        for x in self.nodes[1].listunspent():
            assert (self.nodes[1].lockunspent(False, True, [{"txid": x['txid'], "vout": x['vout']}]))
        # check that it cannot stake
        sleep(1)
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        # create shielded balance for node 0
        self.log.info("Shielding some coins for node0...")
        self.nodes[0].shieldsendmany("from_transparent", [{"address": self.nodes[0].getnewshieldaddress(),
                                                             "amount": Decimal('250.00')}], 1)
        self.sync_all()
        for i in range(6):
            self.mocktime = self.generate_pos(0, self.mocktime)
        self.sync_blocks()
        assert_equal(self.nodes[0].getshieldbalance(), 250)

        # 3) nodes[0] generates a owner address
        #    nodes[1] generates a cold-staking address.
        # ---------------------------------------------
        print("*** 3 ***")
        owner_address = self.nodes[0].getnewaddress()
        self.log.info("Owner Address: %s" % owner_address)
        staker_address = self.nodes[1].getnewstakingaddress()
        staker_privkey = self.nodes[1].dumpprivkey(staker_address)
        assert_equal(self.nodes[1].liststakingaddresses()[0]["address"], staker_address)
        self.log.info("Staking Address: %s" % staker_address)

        # 4) Check enforcement.
        # ---------------------
        print("*** 4 ***")
        # Check that SPORK 17 is disabled
        assert (not self.isColdStakingEnforced())
        self.log.info("Creating a stake-delegation tx before cold staking enforcement...")
        assert_raises_rpc_error(-4, "Failed to accept tx in the memory pool (reason: cold-stake-inactive)\nTransaction canceled.",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, owner_address,
                                False, False, False, True)
        self.log.info("Good. Cold Staking NOT ACTIVE yet.")

        # Enable via SPORK
        self.setColdStakingEnforcement()
        # double check
        assert (self.isColdStakingEnforced())

        # 5) nodes[0] delegates a number of inputs for nodes[1] to stake em.
        # ------------------------------------------------------------------
        print("*** 5 ***")
        self.log.info("First check warning when using external addresses...")
        assert_raises_rpc_error(-5, "Only the owner of the key to owneraddress will be allowed to spend these coins",
                                self.nodes[0].delegatestake, staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT")
        self.log.info("Good. Warning triggered.")

        self.log.info("Now force the use of external address creating (but not sending) the delegation...")
        res = self.nodes[0].rawdelegatestake(staker_address, INPUT_VALUE, "yCgCXC8N5VThhfiaVuKaNLkNnrWduzVnoT", True)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now delegate with internal owner address..")
        self.log.info("Try first with a value (0.99) below the threshold")
        assert_raises_rpc_error(-8, "Invalid amount",
                                self.nodes[0].delegatestake, staker_address, 0.99, owner_address)
        self.log.info("Nice. it was not possible.")
        self.log.info("Then try (creating but not sending) with the threshold value (1.00)")
        res = self.nodes[0].rawdelegatestake(staker_address, 1.00, owner_address)
        assert(res is not None and res != "")
        self.log.info("Good. Warning NOT triggered.")

        self.log.info("Now creating %d real stake-delegation txes..." % NUM_OF_INPUTS)
        for i in range(NUM_OF_INPUTS-1):
            res = self.nodes[0].delegatestake(staker_address, INPUT_VALUE, owner_address)
            assert(res is not None and res["txid"] is not None and res["txid"] != "")
            assert_equal(res["owner_address"], owner_address)
            assert_equal(res["staker_address"], staker_address)
        # delegate  the shielded balance
        res = self.nodes[0].delegatestake(staker_address, INPUT_VALUE, owner_address, False, False, True)
        assert (res is not None and res["txid"] is not None and res["txid"] != "")
        assert_equal(res["owner_address"], owner_address)
        assert_equal(res["staker_address"], staker_address)
        fee = self.nodes[0].viewshieldtransaction(res["txid"])['fee']
        # sync and mine 2 blocks
        self.sync_mempools()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        self.log.info("%d Txes created." % NUM_OF_INPUTS)
        # check balances:
        self.expected_balance = NUM_OF_INPUTS * INPUT_VALUE
        self.expected_immature_balance = 0
        self.checkBalances()
        # also shielded balance of node 0 (250 - 249 - fee)
        assert_equal(self.nodes[0].getshieldbalance(), round(Decimal(1)-Decimal(fee), 8))

        # 6) check that the owner (nodes[0]) can spend the coins.
        # -------------------------------------------------------
        print("*** 6 ***")
        self.log.info("Spending back one of the delegated UTXOs...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_equal(NUM_OF_INPUTS, len(delegated_utxos))
        assert_equal(len(delegated_utxos), len(self.nodes[0].listcoldutxos()))
        u = delegated_utxos[0]
        txhash = self.spendUTXOwithNode(u, 0)
        assert(txhash is not None)
        self.log.info("Good. Owner was able to spend - tx: %s" % str(txhash))
        self.sync_mempools()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        # check tx
        self.check_tx_in_chain(0, txhash)
        self.check_tx_in_chain(1, txhash)
        # check balances after spend.
        self.expected_balance -= float(u["amount"])
        self.checkBalances()
        self.log.info("Balances check out after spend")
        assert_equal(NUM_OF_INPUTS-1, len(self.nodes[0].listcoldutxos()))

        # 7) check that the staker CANNOT use the coins to stake yet.
        # He needs to whitelist the owner first.
        # -----------------------------------------------------------
        print("*** 7 ***")
        self.log.info("Trying to generate a cold-stake block before whitelisting the owner...")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        assert_equal(self.nodes[1].listdelegators(), [])
        self.log.info("Nice. Cold staker was NOT able to create the block yet.")

        self.log.info("Whitelisting the owner...")
        ret = self.nodes[1].delegatoradd(owner_address)
        assert(ret)
        assert_equal(self.nodes[1].listdelegators()[0]["address"], owner_address)
        self.log.info("Delegator address %s whitelisted" % owner_address)

        # 8) check that the staker CANNOT spend the coins.
        # ------------------------------------------------
        print("*** 8 ***")
        self.log.info("Trying to spend one of the delegated UTXOs with the cold-staking key...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        assert_greater_than(len(delegated_utxos), 0)
        u = delegated_utxos[0]
        assert_raises_rpc_error(-26, "mandatory-script-verify-flag-failed (Script failed an OP_CHECKCOLDSTAKEVERIFY operation",
                                self.spendUTXOwithNode, u, 1)
        self.log.info("Good. Cold staker was NOT able to spend (failed OP_CHECKCOLDSTAKEVERIFY)")
        for _ in range(20): # Staking min depth
            self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()

        # 9) check that the staker can use the coins to stake a block with internal miner.
        # --------------------------------------------------------------------------------
        print("*** 9 ***")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], NUM_OF_INPUTS-1)
        self.log.info("Generating one valid cold-stake block...")
        self.mocktime = self.generate_pos(1, self.mocktime)
        self.log.info("New block created by cold-staking. Trying to submit...")
        newblockhash = self.nodes[1].getbestblockhash()
        self.log.info("Block %s submitted" % newblockhash)

        # Verify that nodes[0] accepts it
        self.sync_blocks()
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(newblockhash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")

        # check balances after staked block.
        self.expected_balance -= INPUT_VALUE
        self.expected_immature_balance += (INPUT_VALUE + 250)
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 10) check that the staker can use the coins to stake a block with a rawtransaction.
        # ----------------------------------------------------------------------------------
        print("*** 10 ***")
        self.log.info("Generating another valid cold-stake block...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, staker_privkey)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        assert (ret is None)
        self.log.info("Block %s submitted." % new_block.hash)
        assert_equal(new_block.hash, self.nodes[1].getbestblockhash())

        # Verify that nodes[0] accepts it
        self.sync_blocks()
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        assert_equal(new_block.hash, self.nodes[0].getbestblockhash())
        self.log.info("Great. Cold-staked block was accepted!")
        self.mocktime += 60
        set_node_times(self.nodes, self.mocktime)

        # check balances after staked block.
        self.expected_balance -= INPUT_VALUE
        self.expected_immature_balance += (INPUT_VALUE + 250)
        self.checkBalances()
        self.log.info("Balances check out after staked block")

        # 11) check that the staker cannot stake a block changing the coinstake scriptPubkey.
        # ----------------------------------------------------------------------------------
        print("*** 11 ***")
        self.log.info("Generating one invalid cold-stake block (changing first coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block (with dummy key)
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, "")
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert("rejected" in ret)

        # Verify that nodes[0] rejects it
        self.sync_blocks()
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 12) neither adding different outputs to the coinstake.
        # ------------------------------------------------------
        print("*** 12 ***")
        self.log.info("Generating another invalid cold-stake block (adding coinstake output)...")
        stakeable_coins = getDelegatedUtxos(self.nodes[0].listunspent())
        stakeInputs = self.get_prevouts(1, stakeable_coins)
        assert_greater_than(len(stakeInputs), 0)
        # Create the block
        new_block = self.stake_next_block(1, stakeInputs, self.mocktime, staker_privkey)
        # Add output (dummy key address) to coinstake (taking 100 HMS from the pot)
        self.add_output_to_coinstake(new_block, 100)
        self.log.info("New block created (rawtx) by cold-staking. Trying to submit...")
        # Try to submit the block
        ret = self.nodes[1].submitblock(bytes_to_hex_str(new_block.serialize()))
        self.log.info("Block %s submitted." % new_block.hash)
        assert ret in ["bad-p2cs-outs", "rejected"]

        # Verify that nodes[0] rejects it
        self.sync_blocks()
        assert_raises_rpc_error(-5, "Block not found", self.nodes[0].getblock, new_block.hash)
        self.log.info("Great. Malicious cold-staked block was NOT accepted!")
        self.checkBalances()
        self.log.info("Balances check out after (non) staked block")

        # 13) Now node[0] gets mad and spends all the delegated coins, voiding the P2CS contracts.
        # ----------------------------------------------------------------------------------------
        self.log.info("Let's void the contracts.")
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        print("*** 13 ***")
        self.log.info("Invalidate delegation")
        ret = self.nodes[1].delegatorremove(owner_address)
        assert(ret)
        assert_equal(self.nodes[1].listdelegators(), [])
        assert_equal(self.nodes[1].listdelegators(True)[0]["address"], owner_address)
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        self.log.info("Re-enable delegation")
        ret = self.nodes[1].delegatoradd(owner_address)
        assert(ret)
        assert_equal(self.nodes[1].listdelegators()[0]["address"], owner_address)
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], len(stakeable_coins))
        self.log.info("Cancel the stake delegation spending the delegated utxos...")
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        # remove one utxo to spend later
        final_spend = delegated_utxos.pop()
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert(txhash is not None)
        self.log.info("Good. Owner was able to void the stake delegations - tx: %s" % str(txhash))
        self.sync_blocks()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()

        # deactivate SPORK 17 and check that the owner can still spend the last utxo
        self.setColdStakingEnforcement(False)
        assert (not self.isColdStakingEnforced())
        txhash = self.spendUTXOsWithNode([final_spend], 0)
        assert(txhash is not None)
        self.log.info("Good. Owner was able to void a stake delegation (with SPORK 17 disabled) - tx: %s" % str(txhash))
        self.sync_mempools()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        # check tx
        self.check_tx_in_chain(0, txhash)
        self.check_tx_in_chain(1, txhash)
        # check balances after big spend.
        self.expected_balance = 0
        self.checkBalances()
        self.log.info("Balances check out after the delegations have been voided.")
        # re-activate SPORK17
        self.setColdStakingEnforcement()
        assert (self.isColdStakingEnforced())

        # 14) check that coinstaker is empty and can no longer stake.
        # -----------------------------------------------------------
        print("*** 14 ***")
        self.log.info("Trying to generate one cold-stake block again...")
        assert_equal(self.nodes[1].getstakingstatus()["stakeablecoins"], 0)
        self.log.info("Good. Cold staker was NOT able to create any more blocks.")

        # 15) check balances when mature.
        # -----------------------------------------------------------
        print("*** 15 ***")
        self.log.info("Staking 100 blocks to mature the cold stakes...")
        for i in range(2):
            for peer in [0, 2]:
                for j in range(25):
                    self.mocktime = self.generate_pos(peer, self.mocktime)
                self.sync_blocks()
        self.expected_balance = self.expected_immature_balance
        self.expected_immature_balance = 0
        self.checkBalances()
        delegated_utxos = getDelegatedUtxos(self.nodes[0].listunspent())
        txhash = self.spendUTXOsWithNode(delegated_utxos, 0)
        assert (txhash is not None)
        self.log.info("Good. Owner was able to spend the cold staked coins - tx: %s" % str(txhash))
        self.sync_mempools()
        self.mocktime = self.generate_pos(2, self.mocktime)
        self.sync_blocks()
        # check tx
        self.check_tx_in_chain(0, txhash)
        self.check_tx_in_chain(1, txhash)
        self.expected_balance = 0
        self.checkBalances()


    def checkBalances(self):
        w_info = self.nodes[0].getwalletinfo()
        assert_equal(self.nodes[0].getblockcount(), w_info['last_processed_block'])
        self.log.info("OWNER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_delegated_balance"]), self.expected_immature_balance)
        assert_equal(float(w_info["cold_staking_balance"]), 0)
        w_info = self.nodes[1].getwalletinfo()
        assert_equal(self.nodes[1].getblockcount(), w_info['last_processed_block'])
        self.log.info("STAKER - Delegated %f / Cold %f   [%f / %f]" % (
            float(w_info["delegated_balance"]), w_info["cold_staking_balance"],
            float(w_info["immature_delegated_balance"]), w_info["immature_cold_staking_balance"]))
        assert_equal(float(w_info["delegated_balance"]), 0)
        assert_equal(float(w_info["cold_staking_balance"]), self.expected_balance)
        assert_equal(float(w_info["immature_cold_staking_balance"]), self.expected_immature_balance)

    def spendUTXOwithNode(self, utxo, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        out_amount = (float(utxo["amount"]) - self.DEFAULT_FEE)
        outputs = {}
        outputs[new_addy] = out_amount
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def spendUTXOsWithNode(self, utxos, node_n):
        new_addy = self.nodes[node_n].getnewaddress()
        inputs = []
        outputs = {}
        outputs[new_addy] = 0
        for utxo in utxos:
            inputs.append({"txid": utxo["txid"], "vout": utxo["vout"]})
            outputs[new_addy] += float(utxo["amount"])
        outputs[new_addy] -= self.DEFAULT_FEE
        spendingTx = self.nodes[node_n].createrawtransaction(inputs, outputs)
        spendingTx_signed = self.nodes[node_n].signrawtransaction(spendingTx)
        return self.nodes[node_n].sendrawtransaction(spendingTx_signed["hex"])

    def add_output_to_coinstake(self, block, value, peer=1):
        coinstake = block.vtx[1]
        if not hasattr(self, 'DUMMY_KEY'):
            self.init_dummy_key()
        coinstake.vout.append(
            CTxOut(value * COIN, CScript([self.DUMMY_KEY.get_pubkey().get_bytes(), OP_CHECKSIG])))
        coinstake.vout[1].nValue -= value * COIN
        # re-sign coinstake
        prevout = COutPoint()
        prevout.deserialize_uniqueness(BytesIO(block.prevoutStake))
        coinstake.vin[0] = CTxIn(prevout)
        stake_tx_signed_raw_hex = self.nodes[peer].signrawtransaction(
            bytes_to_hex_str(coinstake.serialize()))['hex']
        block.vtx[1] = CTransaction()
        block.vtx[1].from_hex(stake_tx_signed_raw_hex)
        # re-sign block
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()
        block.re_sign_block()


if __name__ == '__main__':
    hemis_ColdStakingTest().main()
