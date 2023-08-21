#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test deterministic gamemasters"""

from decimal import Decimal
from random import randrange, getrandbits
import time

from test_framework.blocktools import create_block, create_coinbase
from test_framework.messages import CTxOut, COIN
from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_greater_than,
    assert_equal,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    create_new_dgm,
    connect_nodes,
    hex_str_to_bytes,
    is_coin_locked_by,
    spend_gm_collateral,
)


class DIP3Test(HemisTestFramework):

    def set_test_params(self):
        # 1 miner, 1 controller, 6 remote gms
        self.num_nodes = 8
        self.minerPos = 0
        self.controllerPos = 1
        self.setup_clean_chain = True
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:130"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def add_new_dgm(self, gms, strType, op_keys=None, from_out=None):
        gms.append(self.register_new_dgm(2 + len(gms),
                                         self.minerPos,
                                         self.controllerPos,
                                         strType,
                                         outpoint=from_out,
                                         op_blskeys=op_keys))

    def check_gm_list(self, gms):
        for i in range(self.num_nodes):
            self.check_gm_list_on_node(i, gms)
        self.log.info("Deterministic list contains %d gamemasters for all peers." % len(gms))

    def check_gm_enabled_count(self, enabled, total):
        for node in self.nodes:
            node_count = node.getgamemastercount()
            assert_equal(node_count['enabled'], enabled)
            assert_equal(node_count['total'], total)

    def get_addr_balance(self, node, addr):
        rcv = node.listreceivedbyaddress(0, False, False, addr)
        return rcv[0]['amount'] if len(rcv) > 0 else 0

    def get_last_paid_gm(self):
        return next(x['proTxHash'] for x in self.nodes[0].listgamemasters()
                    if x['dgmstate']['lastPaidHeight'] == self.nodes[0].getblockcount())

    def create_block(self, gm_payee_script, prev_block):
        coinbase = create_coinbase(prev_block["height"] + 1)
        coinbase.vout[0].nValue -= 3 * COIN
        coinbase.vout.append(CTxOut(int(3 * COIN), hex_str_to_bytes(gm_payee_script)))
        coinbase.rehash()
        return create_block(int(prev_block["hash"], 16),
                            coinbase,
                            hashFinalSaplingRoot=int(prev_block["finalsaplingroot"], 16),
                            nVersion=10)

    def restart_controller(self):
        self.restart_node(self.controllerPos, extra_args=self.extra_args[self.controllerPos])
        self.connect_to_all(self.controllerPos)
        connect_nodes(self.nodes[self.controllerPos], self.minerPos)
        self.sync_all()

    def wait_until_gmsync_completed(self):
        SYNC_FINISHED = [999] * self.num_nodes
        synced = [-1] * self.num_nodes
        timeout = time.time() + 120
        while synced != SYNC_FINISHED and time.time() < timeout:
            synced = [node.gmsync("status")["RequestedGamemasterAssets"]
                      for node in self.nodes]
            if synced != SYNC_FINISHED:
                time.sleep(5)
        if synced != SYNC_FINISHED:
            raise AssertionError("Unable to complete gmsync: %s" % str(synced))

    def run_test(self):
        self.disable_mocktime()

        # Additional connections to miner and owner
        for nodePos in [self.minerPos, self.controllerPos]:
            self.connect_to_all(nodePos)
        miner = self.nodes[self.minerPos]
        controller = self.nodes[self.controllerPos]

        dummy_add = controller.getnewaddress("dummy")

        # Enforce gm payments and reject legacy gms at block 131
        self.activate_spork(0, "SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT")
        assert_equal("success", self.set_spork(self.minerPos, "SPORK_21_LEGACY_GMS_MAX_HEIGHT", 130))
        time.sleep(1)
        assert_equal([130] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_GMS_MAX_HEIGHT")
                                              for x in range(self.num_nodes)])
        gms = []

        # Mine 100 blocks
        self.log.info("Mining...")
        miner.generate(110)
        self.sync_blocks()
        self.assert_equal_for_all(110, "getblockcount")

        # Test rejection before enforcement
        self.log.info("Testing rejection of ProRegTx before DIP3 activation...")
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", self.add_new_dgm, gms, "internal")
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", self.add_new_dgm, gms, "fund")
        # Can create the raw proReg
        dgm = create_new_dgm(2, controller, dummy_add, None)
        tx, sig = self.protx_register_ext(miner, controller, dgm, None, False)
        # but cannot send it
        assert_raises_rpc_error(-1, "Evo upgrade is not active yet", miner.protx_register_submit, tx, sig)
        self.log.info("Done. Now mine blocks till enforcement...")

        # Check that no coin has been locked by the controller yet
        assert_equal(len(controller.listlockunspent()["transparent"]), 0)

        # DIP3 activates at block 130.
        miner.generate(130 - miner.getblockcount())
        self.sync_blocks()
        self.assert_equal_for_all(130, "getblockcount")

        # -- DIP3 enforced and SPORK_21 active here --
        self.wait_until_gmsync_completed()

        # enabled/total gamemasters: 0/0
        self.check_gm_enabled_count(0, 0)

        # Create 3 DGMs and init the remote nodes
        self.log.info("Initializing gamemasters...")
        self.add_new_dgm(gms, "internal")
        self.add_new_dgm(gms, "external")
        self.add_new_dgm(gms, "fund")
        for gm in gms:
            self.nodes[gm.idx].initgamemaster(gm.operator_sk)
            time.sleep(1)
        miner.generate(1)
        self.sync_blocks()

        # enabled/total gamemasters: 3/3
        self.check_gm_enabled_count(3, 3)

        # Init the other 3 remote nodes before creating the ProReg tx
        self.log.info("Initializing more gamemasters...")
        op_keys = []
        for i in range(3):
            idx = 2 + len(gms) + i
            bls_keypair = controller.generateblskeypair()
            self.nodes[idx].initgamemaster(bls_keypair["secret"])
            op_keys.append([bls_keypair["public"], bls_keypair["secret"]])
            time.sleep(1)

        # Now send the ProReg txes and check list
        self.add_new_dgm(gms, "internal", op_keys[0])
        self.add_new_dgm(gms, "external", op_keys[1])
        self.add_new_dgm(gms, "fund", op_keys[2])
        miner.generate(2)
        self.sync_blocks()
        time.sleep(1)
        self.log.info("Gamemasters started.")

        # enabled/total gamemasters: 6/6
        self.check_gm_enabled_count(6, 6)
        self.check_gm_list(gms)

        # Check status from remote nodes
        assert_equal([self.nodes[idx].getgamemasterstatus()['status'] for idx in range(2, self.num_nodes)],
                     ["Ready"] * (self.num_nodes - 2))
        self.log.info("All gamemasters ready.")

        # Restart the controller and check that the collaterals are still locked
        self.log.info("Restarting controller...")
        self.restart_controller()
        time.sleep(1)
        for gm in gms:
            if not is_coin_locked_by(controller, gm.collateral):
                raise Exception(
                    "Collateral %s of gm with idx=%d is not locked" % (gm.collateral, gm.idx)
                )
        self.log.info("Collaterals still locked.")

        # Test collateral spending
        dgm = gms.pop(randrange(len(gms)))  # pop one at random
        self.log.info("Spending collateral of gm with idx=%d..." % dgm.idx)
        spend_txid = spend_gm_collateral(controller, dgm)
        self.sync_mempools([miner, controller])
        miner.generate(1)
        self.sync_blocks()
        assert_greater_than(miner.getrawtransaction(spend_txid, True)["confirmations"], 0)

        # enabled/total gamemasters: 5/5
        self.check_gm_enabled_count(5, 5)
        self.check_gm_list(gms)

        # Register dgm again, with the collateral of dgm2
        # dgm must be added again to the list, and dgm2 must be removed
        dgm2 = gms.pop(randrange(len(gms)))  # pop one at random
        dgm_keys = [dgm.operator_pk, dgm.operator_sk]
        dgm2_keys = [dgm2.operator_pk, dgm2.operator_sk]
        self.log.info("Reactivating node %d reusing the collateral of node %d..." % (dgm.idx, dgm2.idx))
        gms.append(self.register_new_dgm(dgm.idx, self.minerPos, self.controllerPos, "external",
                                         outpoint=dgm2.collateral, op_blskeys=dgm_keys))
        miner.generate(1)
        self.sync_blocks()

        # enabled/total gamemasters: 5/5
        self.check_gm_enabled_count(5, 5)
        self.check_gm_list(gms)

        # Now try to register dgm2 again with an already-used IP
        self.log.info("Trying duplicate IP...")
        rand_idx = gms[randrange(len(gms))].idx
        assert_raises_rpc_error(-1, "bad-protx-dup-IP-address",
                                self.register_new_dgm, rand_idx, self.minerPos, self.controllerPos, "fund",
                                op_blskeys=dgm2_keys)

        # Now try with duplicate operator key
        self.log.info("Trying duplicate operator key...")
        dgm2b = create_new_dgm(dgm2.idx, controller, dummy_add, dgm_keys)
        assert_raises_rpc_error(-1, "bad-protx-dup-operator-key",
                                self.protx_register_fund, miner, controller, dgm2b, dummy_add)

        # Now try with duplicate owner key
        self.log.info("Trying duplicate owner key...")
        dgm2c = create_new_dgm(dgm2.idx, controller, dummy_add, dgm2_keys)
        dgm2c.owner = gms[randrange(len(gms))].owner
        assert_raises_rpc_error(-1, "bad-protx-dup-owner-key",
                                self.protx_register_fund, miner, controller, dgm2c, dummy_add)

        # Finally, register it properly. This time setting 10% of the reward for the operator
        op_rew = {"reward": 10.00, "address": self.nodes[dgm2.idx].getnewaddress()}
        self.log.info("Reactivating the node with a new registration (with operator reward)...")
        dgm2c = create_new_dgm(dgm2.idx, controller, dummy_add, dgm2_keys)
        self.protx_register_fund(miner, controller, dgm2c, dummy_add, op_rew)
        gms.append(dgm2c)
        time.sleep(1)
        self.sync_mempools([miner, controller])
        miner.generate(6)
        self.sync_blocks()
        json_tx = self.nodes[dgm2c.idx].getrawtransaction(dgm2c.proTx, True)
        assert_greater_than(json_tx['confirmations'], 0)
        self.check_proreg_payload(dgm2c, json_tx)

        # enabled/total gamemasters: 6/6
        self.check_gm_enabled_count(6, 6)
        self.check_gm_list(gms)     # 6 gamemasters again

        # Test payments.
        # Mine 12 blocks and check that each gamemaster has been paid exactly twice.
        # Save last paid gamemaster. Check that it's the last paid also after the 12 blocks.
        # Note: dgm2 sends (2 * 0.3 HMS) to the operator, and (2 * 2.7 HMS) to the owner
        self.log.info("Testing gamemaster payments...")
        last_paid_gm = self.get_last_paid_gm()
        starting_balances = {"operator": self.get_addr_balance(self.nodes[dgm2c.idx], op_rew["address"])}
        for gm in gms:
            starting_balances[gm.payee] = self.get_addr_balance(controller, gm.payee)
        miner.generate(12)
        self.sync_blocks()
        for gm in gms:
            bal = self.get_addr_balance(controller, gm.payee)
            expected = starting_balances[gm.payee] + (Decimal('6.0') if gm.idx != dgm2c.idx else Decimal('5.4'))
            if bal != expected:
                raise Exception("Invalid balance (%s != %s) for node %d" % (bal, expected, gm.idx))
        self.log.info("All gamemasters paid twice.")
        assert_equal(self.get_addr_balance(self.nodes[dgm2c.idx], op_rew["address"]),
                     starting_balances["operator"] + Decimal('0.6'))
        self.log.info("Operator paid twice.")
        assert_equal(last_paid_gm, self.get_last_paid_gm())
        self.log.info("Order preserved.")

        # Test invalid payment
        self.wait_until_gmsync_completed()   # just to be sure
        self.log.info("Testing invalid gamemaster payment...")
        gm_payee_script = miner.validateaddress(miner.getnewaddress())['scriptPubKey']
        block = self.create_block(gm_payee_script, miner.getblock(miner.getbestblockhash(), True))
        block.solve()
        assert_equal(miner.submitblock(bytes_to_hex_str(block.serialize())), "bad-cb-payee")

        # Test ProUpServ txes
        self.log.info("Trying to update a non-existent gamemaster...")
        assert_raises_rpc_error(-8, "not found", miner.protx_update_service,
                                "%064x" % getrandbits(256), "127.0.0.1:1000")
        self.log.info("Trying to update an IP address to an already used one...")
        assert_raises_rpc_error(-1, "bad-protx-dup-addr", miner.protx_update_service,
                                gms[0].proTx, gms[1].ipport, "", gms[0].operator_sk)
        self.log.info("Trying to update the payout address when the reward is 0...")
        assert_raises_rpc_error(-8, "Operator reward is 0. Cannot set operator payout address",
                                miner.protx_update_service, gms[0].proTx, "",
                                miner.getnewaddress(), gms[0].operator_sk)
        self.log.info("Trying to update the operator payee to an invalid address...")
        assert_raises_rpc_error(-5, "invalid hemis address InvalidPayee",
                                miner.protx_update_service, dgm2c.proTx, "", "InvalidPayee", "")
        self.log.info("Update IP address...")
        gms[0].ipport = "127.0.0.1:1000"
        # Do it from the remote node (so no need to pass the operator BLS secret key)
        remote_node = self.nodes[gms[0].idx]
        # Send first some funds
        miner.sendtoaddress(remote_node.getnewaddress(), 1.0)
        miner.generate(1)
        self.sync_blocks()
        # Then send the ProUpServ tx from the gamemaster
        remote_node.protx_update_service(gms[0].proTx, gms[0].ipport)
        self.sync_mempools([miner, remote_node])
        miner.generate(1)
        self.sync_blocks()
        self.check_gm_list(gms)
        self.log.info("Update operator payout address...")
        # This time send the ProUpServ tx directly from the miner, giving the operator BLS secret key
        new_address = self.nodes[dgm2c.idx].getnewaddress()
        miner.protx_update_service(dgm2c.proTx, dgm2c.ipport, new_address, dgm2c.operator_sk)
        miner.generate(len(gms) + 1)
        self.sync_blocks()
        # Check payment to new address
        self.log.info("Checking payment...")
        assert_equal(self.get_addr_balance(self.nodes[dgm2c.idx], new_address), Decimal('0.3'))

        # Test ProUpReg txes
        self.log.info("Trying to update a non-existent gamemaster...")
        assert_raises_rpc_error(-8, "not found", miner.protx_update_registrar,
                                "%064x" % getrandbits(256), "", "", "")
        self.log.info("Trying to update an operator address to an already used one...")
        assert_raises_rpc_error(-1, "bad-protx-dup-key", controller.protx_update_registrar,
                                gms[0].proTx, gms[1].operator_pk, "", "")
        self.log.info("Trying to update the payee to an invalid address...")
        assert_raises_rpc_error(-5, "invalid hemis address InvalidPayee", controller.protx_update_registrar,
                                gms[0].proTx, "", "", "InvalidPayee")
        self.log.info("Update operator keys...")
        bls_keypair = self.nodes[gms[0].idx].generateblskeypair()
        gms[0].operator_pk = bls_keypair["public"]
        gms[0].operator_sk = bls_keypair["secret"]
        # Controller should already have the key (as it was generated there), no need to pass it
        controller.protx_update_registrar(gms[0].proTx, gms[0].operator_pk, "", "")
        self.sync_mempools([miner, controller])
        miner.generate(1)
        self.sync_blocks()

        # enabled/total gamemasters: 5/6
        # Updating the operator key, clears the IP (and puts the gm in PoSe banned state)
        self.check_gm_enabled_count(5, 6)
        gms[0].ipport = "[::]:0"
        self.check_gm_list(gms)

        old_gm0_balance = self.get_addr_balance(controller, gms[0].payee)
        self.log.info("Update operator address (with external key)...")
        bls_keypair = self.nodes[gms[0].idx].generateblskeypair()
        gms[0].operator_pk = bls_keypair["public"]
        gms[0].operator_sk = bls_keypair["secret"]
        ownerKey = controller.dumpprivkey(gms[0].owner)
        miner.protx_update_registrar(gms[0].proTx, gms[0].operator_pk, "", "", ownerKey)
        miner.generate(1)
        self.sync_blocks()
        self.check_gm_enabled_count(5, 6) # stil not valid until new operator sends proUpServ
        self.check_gm_list(gms)
        self.log.info("Update voting address...")
        gms[1].voting = controller.getnewaddress()
        controller.protx_update_registrar(gms[1].proTx, "", gms[1].voting, "")
        self.sync_mempools([miner, controller])
        miner.generate(1)
        self.sync_blocks()
        self.check_gm_enabled_count(5, 6)
        self.check_gm_list(gms)
        self.log.info("Update payout address...")
        old_payee = gms[2].payee
        gms[2].payee = controller.getnewaddress()
        controller.protx_update_registrar(gms[2].proTx, "", "", gms[2].payee)
        self.sync_mempools([miner, controller])
        miner.generate(1)
        self.sync_blocks()
        old_gm2_bal = self.get_addr_balance(controller, old_payee)
        miner.generate(len(gms)-1)
        self.sync_blocks()
        self.check_gm_enabled_count(5, 6)
        self.check_gm_list(gms)
        # Check payment to new address
        self.log.info("Checking payments...")
        assert_equal(self.get_addr_balance(controller, old_payee), old_gm2_bal)
        assert_equal(self.get_addr_balance(controller, gms[2].payee), Decimal('3'))
        # The PoSe banned node didn't receive any more payment
        assert_equal(self.get_addr_balance(controller, gms[0].payee), old_gm0_balance)

        # Test ProUpRev txes
        self.log.info("Trying to revoke a non-existent gamemaster...")
        assert_raises_rpc_error(-8, "not found", miner.protx_revoke,
                                "%064x" % getrandbits(256))
        self.log.info("Trying to revoke with invalid reason...")
        assert_raises_rpc_error(-8, "invalid reason", controller.protx_revoke, gms[3].proTx, gms[3].operator_sk, 100)
        self.log.info("Revoke gamemaster...")
        # Do it from the remote node (so no need to pass the operator BLS secret key)
        remote_node = self.nodes[gms[3].idx]
        # Send first some funds
        miner.sendtoaddress(remote_node.getnewaddress(), 1.0)
        miner.generate(1)
        self.sync_blocks()
        # Then send the ProUpRev tx from the gamemaster
        remote_node.protx_revoke(gms[3].proTx, "", 1)
        gms[3].revoked()
        self.sync_mempools([miner, remote_node])
        miner.generate(1)
        self.sync_blocks()
        self.check_gm_enabled_count(4, 6)   # gm3 has been revoked
        self.check_gm_list(gms)
        old_gm3_bal = self.get_addr_balance(controller, gms[3].payee)
        # This time send the ProUpRev tx directly from the miner, giving the operator BLS secret key
        self.log.info("Revoke gamemaster (with external key)...")
        miner.protx_revoke(gms[4].proTx, gms[4].operator_sk, 2)
        gms[4].revoked()
        miner.generate(1)
        self.sync_blocks()
        self.check_gm_list(gms)
        old_gm4_bal = self.get_addr_balance(controller, gms[4].payee)
        miner.generate(len(gms) + 1)
        self.sync_blocks()

        # enabled/total gamemasters: 3/6 (gm0 banned, gm3 and gm4 revoked)
        self.check_gm_enabled_count(3, 6)
        self.check_gm_list(gms)

        # Check (no) payments
        self.log.info("Checking payments...")
        assert_equal(self.get_addr_balance(controller, gms[3].payee), old_gm3_bal)
        assert_equal(self.get_addr_balance(controller, gms[4].payee), old_gm4_bal)

        # Test reviving a gamemaster
        self.log.info("Reviving a gamemaster...")
        bls_keypair = controller.generateblskeypair()
        gms[3].operator_pk = bls_keypair["public"]
        gms[3].operator_sk = bls_keypair["secret"]
        miner.protx_update_registrar(gms[3].proTx, gms[3].operator_pk, "", "", controller.dumpprivkey(gms[3].owner))
        miner.generate(1)
        gms[3].ipport = "127.0.0.1:3000"
        miner.protx_update_service(gms[3].proTx, gms[3].ipport, "", gms[3].operator_sk)
        miner.generate(len(gms))
        self.sync_blocks()

        # enabled/total gamemasters: 4/6 (gm3 is back)
        self.check_gm_enabled_count(4, 6)
        self.check_gm_list(gms)

        self.log.info("Checking payments...")
        assert_equal(self.get_addr_balance(controller, gms[3].payee), old_gm3_bal + Decimal('3'))

        self.log.info("All good.")


if __name__ == '__main__':
    DIP3Test().main()
