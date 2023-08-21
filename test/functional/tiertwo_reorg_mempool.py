#!/usr/bin/env python3
# Copyright (c) 2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test deterministic gamemasters conflicts and reorgs.
- Check that in-mempool reuse of gm unique-properties is invalid
- Check mempool eviction after conflict with newly connected block / reorg
- Check deterministic list consensus after reorg
"""

from decimal import Decimal
import random
import time

from test_framework.messages import COutPoint
from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    create_new_dgm,
    connect_nodes,
    disconnect_nodes,
    get_collateral_vout,
)


class TiertwoReorgMempoolTest(HemisTestFramework):

    def set_test_params(self):
        # two nodes mining on separate chains
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:160"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def setup_network(self):
        self.setup_nodes()
        self.connect_all()

    def connect_all(self):
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)

    def disconnect_all(self):
        self.log.info("Disconnecting nodes...")
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 0)
        self.log.info("Nodes disconnected")

    def register_gamemaster(self, from_node, dgm, collateral_addr):
        dgm.proTx = from_node.protx_register_fund(collateral_addr, dgm.ipport, dgm.owner,
                                                  dgm.operator_pk, dgm.voting, dgm.payee)
        dgm.collateral = COutPoint(int(dgm.proTx, 16),
                                   get_collateral_vout(from_node.getrawtransaction(dgm.proTx, True)))

    def run_test(self):
        self.disable_mocktime()
        nodeA = self.nodes[0]
        nodeB = self.nodes[1]
        free_idx = 1  # unique id for gamemasters. first available.

        # Enforce gm payments and reject legacy gms at block 202
        self.activate_spork(0, "SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT")
        assert_equal("success", self.set_spork(0, "SPORK_21_LEGACY_GMS_MAX_HEIGHT", 201))
        time.sleep(1)
        assert_equal([201] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_GMS_MAX_HEIGHT")
                                              for x in range(self.num_nodes)])

        # Mine 201 blocks
        self.log.info("Mining...")
        nodeA.generate(25)
        self.sync_blocks()
        nodeB.generate(25)
        self.sync_blocks()
        nodeA.generate(50)
        self.sync_blocks()
        nodeB.generate(101)
        self.sync_blocks()
        self.assert_equal_for_all(201, "getblockcount")

        # Register two gamemasters before the split
        collateral_addr = nodeA.getnewaddress() # for both collateral and payouts
        pre_split_gm1 = create_new_dgm(100, nodeA, nodeA.getnewaddress(), None)
        pre_split_gm2 = create_new_dgm(200, nodeA, nodeA.getnewaddress(), None)
        self.register_gamemaster(nodeA, pre_split_gm1, collateral_addr)
        self.register_gamemaster(nodeA, pre_split_gm2, collateral_addr)
        nodeA.generate(1)
        self.sync_blocks()
        gmsA = [pre_split_gm1, pre_split_gm2]
        gmsB = [pre_split_gm1, pre_split_gm2]
        self.check_gm_list_on_node(0, gmsA)
        self.check_gm_list_on_node(1, gmsB)
        self.log.info("Pre-split gamemasters registered.")

        # Disconnect the nodes
        self.disconnect_all()   # network splits at block 203

        #
        # -- CHAIN A --
        #

        # Register 5 gamemasters, then mine 5 blocks
        self.log.info("Registering gamemasters on chain A...")
        for _ in range(5):
            dgm = create_new_dgm(free_idx, nodeA, collateral_addr, None)
            free_idx += 1
            self.register_gamemaster(nodeA, dgm, collateral_addr)
            gmsA.append(dgm)
        nodeA.generate(5)
        self.check_gm_list_on_node(0, gmsA)
        self.log.info("Gamemasters registered on chain A.")

        # Now create a collateral (which will be used later to register a gamemaster)
        funding_txid = nodeA.sendtoaddress(collateral_addr, Decimal('100'))
        nodeA.generate(1)
        funding_tx_json = nodeA.getrawtransaction(funding_txid, True)
        assert_greater_than(funding_tx_json["confirmations"], 0)
        initial_collateral = COutPoint(int(funding_txid, 16), get_collateral_vout(funding_tx_json))

        # Lock any utxo with less than 106 confs (e.g. change), so we can resurrect everything
        for x in nodeA.listunspent(0, 106):
            nodeA.lockunspent(False, True, [{"txid": x["txid"], "vout": x["vout"]}])

        # Now send a valid proReg tx to the mempool, without mining it
        mempool_dgm1 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        self.register_gamemaster(nodeA, mempool_dgm1, collateral_addr)
        assert mempool_dgm1.proTx in nodeA.getrawmempool()

        # Try sending a proReg tx with same owner
        self.log.info("Testing in-mempool duplicate-owner rejection...")
        dgm_A1 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dgm_A1.owner = mempool_dgm1.owner
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_gamemaster, nodeA, dgm_A1, collateral_addr)
        assert dgm_A1.proTx not in nodeA.getrawmempool()

        # Try sending a proReg tx with same operator
        self.log.info("Testing in-mempool duplicate-operator rejection...")
        dgm_A2 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dgm_A2.operator_pk = mempool_dgm1.operator_pk
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_gamemaster, nodeA, dgm_A2, collateral_addr)
        assert dgm_A2.proTx not in nodeA.getrawmempool()

        # Try sending a proReg tx with same IP
        self.log.info("Testing proReg in-mempool duplicate-IP rejection...")
        dgm_A3 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dgm_A3.ipport = mempool_dgm1.ipport
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_gamemaster, nodeA, dgm_A3, collateral_addr)
        assert dgm_A3.proTx not in nodeA.getrawmempool()

        # Now send other 2 valid proReg tx to the mempool, without mining them
        self.log.info("Sending more ProReg txes to the mempool...")
        mempool_dgm2 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        mempool_dgm3 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        self.register_gamemaster(nodeA, mempool_dgm2, collateral_addr)
        self.register_gamemaster(nodeA, mempool_dgm3, collateral_addr)

        # Send to the mempool a ProRegTx using the collateral mined after the split
        mempool_dgm4 = create_new_dgm(free_idx, nodeA, collateral_addr, None)
        mempool_dgm4.collateral = initial_collateral
        self.protx_register_ext(nodeA, nodeA, mempool_dgm4, mempool_dgm4.collateral, True)

        # Now send a valid proUpServ tx to the mempool, without mining it
        proupserv1_txid = nodeA.protx_update_service(pre_split_gm1.proTx,
                                                     "127.0.0.1:1000", "", pre_split_gm1.operator_sk)

        # Try sending another update, reusing the same ip of the previous mempool tx
        self.log.info("Testing proUpServ in-mempool duplicate-IP rejection...")
        assert_raises_rpc_error(-26, "protx-dup", nodeA.protx_update_service,
                                gmsA[0].proTx, "127.0.0.1:1000", "", gmsA[0].operator_sk)

        # Now send other two valid proUpServ txes to the mempool, without mining them
        proupserv2_txid = nodeA.protx_update_service(gmsA[3].proTx,
                                                     "127.0.0.1:2000", "", gmsA[3].operator_sk)
        proupserv3_txid = nodeA.protx_update_service(pre_split_gm1.proTx,
                                                     "127.0.0.1:1001", "", pre_split_gm1.operator_sk)

        # Send valid proUpReg tx to the mempool
        operator_to_reuse = nodeA.generateblskeypair()["public"]
        proupreg1_txid = nodeA.protx_update_registrar(gmsA[4].proTx, operator_to_reuse, "", "")

        # Try sending another one, reusing the operator key used by another mempool proTx
        self.log.info("Testing proUpReg in-mempool duplicate-operator-key rejection...")
        assert_raises_rpc_error(-26, "protx-dup", nodeA.protx_update_registrar,
                                gmsA[5].proTx, mempool_dgm1.operator_pk, "", "")

        # Now send other two valid proUpServ txes to the mempool, without mining them
        new_voting_address = nodeA.getnewaddress()
        proupreg2_txid = nodeA.protx_update_registrar(gmsA[5].proTx, "", new_voting_address, "")
        proupreg3_txid = nodeA.protx_update_registrar(pre_split_gm1.proTx, "", new_voting_address, "")

        # Send two valid proUpRev txes to the mempool, without mining them
        self.log.info("Revoking two gamemasters...")
        prouprev1_txid = nodeA.protx_revoke(gmsA[6].proTx, gmsA[6].operator_sk)
        prouprev2_txid = nodeA.protx_revoke(pre_split_gm2.proTx, pre_split_gm2.operator_sk)

        # Now nodeA has 4 proReg txes in its mempool, 3 proUpServ txes, 3 proUpReg txes, and 2 proUpRev
        mempoolA = nodeA.getrawmempool()
        assert mempool_dgm1.proTx in mempoolA
        assert mempool_dgm2.proTx in mempoolA
        assert mempool_dgm3.proTx in mempoolA
        assert mempool_dgm4.proTx in mempoolA
        assert proupserv1_txid in mempoolA
        assert proupserv2_txid in mempoolA
        assert proupserv3_txid in mempoolA
        assert proupreg1_txid in mempoolA
        assert proupreg2_txid in mempoolA
        assert proupreg3_txid in mempoolA
        assert prouprev1_txid in mempoolA
        assert prouprev2_txid in mempoolA

        assert_equal(nodeA.getblockcount(), 208)

        #
        # -- CHAIN B --
        #
        collateral_addr = nodeB.getnewaddress()
        self.log.info("Registering gamemasters on chain B...")

        # Register first the 3 nodes that conflict with the mempool of nodes[0]
        # mine one block after each registration
        for dgm in [dgm_A1, dgm_A2, dgm_A3]:
            self.register_gamemaster(nodeB, dgm, collateral_addr)
            gmsB.append(dgm)
            nodeB.generate(1)
        self.check_gm_list_on_node(1, gmsB)

        # Pick the proReg for the first GM registered on chain A, and replay it on chain B
        self.log.info("Replaying a gamemaster on a different chain...")
        gmsA.remove(pre_split_gm1)
        gmsA.remove(pre_split_gm2)
        replay_gm = gmsA.pop(0)
        gmsB.append(replay_gm)  # same proTx hash
        nodeB.sendrawtransaction(nodeA.getrawtransaction(replay_gm.proTx, False))
        nodeB.generate(1)
        self.check_gm_list_on_node(1, gmsB)

        # Now pick a proReg for another GM registered on chain A, and re-register it on chain B
        self.log.info("Re-registering a gamemaster on a different chain...")
        rereg_gm = random.choice(gmsA)
        gmsA.remove(rereg_gm)
        self.register_gamemaster(nodeB, rereg_gm, collateral_addr)
        gmsB.append(rereg_gm)   # changed proTx hash
        nodeB.generate(1)
        self.check_gm_list_on_node(1, gmsB)

        # Register 5 more gamemasters. One per block.
        for _ in range(5):
            dgm = create_new_dgm(free_idx, nodeB, collateral_addr, None)
            free_idx += 1
            self.register_gamemaster(nodeB, dgm, collateral_addr)
            gmsB.append(dgm)
            nodeB.generate(1)
        self.check_gm_list_on_node(1, gmsB)

        # Register one gamemaster reusing the IP of the proUpServ mempool tx on chainA
        dgm1000 = create_new_dgm(free_idx, nodeB, collateral_addr, None)
        free_idx += 1
        dgm1000.ipport = "127.0.0.1:1000"
        gmsB.append(dgm1000)
        self.register_gamemaster(nodeB, dgm1000, collateral_addr)

        # Register one gamemaster reusing the operator-key of the proUpReg mempool tx on chainA
        dgmop = create_new_dgm(free_idx, nodeB, collateral_addr, None)
        free_idx += 1
        dgmop.operator_pk = operator_to_reuse
        gmsB.append(dgmop)
        self.register_gamemaster(nodeB, dgmop, collateral_addr)

        # Then mine 10 more blocks on chain B
        nodeB.generate(10)
        self.check_gm_list_on_node(1, gmsB)
        self.log.info("Gamemasters registered on chain B.")

        assert_equal(nodeB.getblockcount(), 222)

        #
        # -- RECONNECT --
        #

        # Reconnect and sync (give it some more time)
        self.log.info("Reconnecting nodes...")
        self.connect_all()
        self.sync_blocks(wait=3, timeout=180)

        # Both nodes have the same list (gmB)
        self.log.info("Checking gamemaster list...")
        self.check_gm_list_on_node(0, gmsB)
        self.check_gm_list_on_node(1, gmsB)

        self.log.info("Checking mempool...")
        mempoolA = nodeA.getrawmempool()
        # The first mempool proReg tx has been removed from nodeA's mempool due to
        # conflicts with the gamemasters of chain B, now connected.
        # The fourth mempool proReg tx has been removed because the collateral it
        # was referencing has been disconnected.
        assert mempool_dgm1.proTx not in mempoolA
        assert mempool_dgm2.proTx in mempoolA
        assert mempool_dgm3.proTx in mempoolA
        assert mempool_dgm4.proTx not in mempoolA
        # The first mempool proUpServ tx has been removed as the IP (port=1000) is
        # now used by a newly connected gamemaster.
        # The second mempool proUpServ tx has been removed as it was meant to update
        # a gamemaster that is not in the deterministic list anymore.
        assert proupserv1_txid not in mempoolA
        assert proupserv2_txid not in mempoolA
        assert proupserv3_txid in mempoolA
        # The first mempool proUpReg tx has been removed as the operator key is
        # now used by a newly connected gamemaster.
        # The second mempool proUpReg tx has been removed as it was meant to update
        # a gamemaster that is not in the deterministic list anymore.
        assert proupreg1_txid not in mempoolA
        assert proupreg2_txid not in mempoolA
        assert proupreg3_txid in mempoolA
        # The frist mempool proUpRev tx has been removed as it was meant to revoke
        # a gamemaster that is not in the deterministic list anymore.
        assert prouprev1_txid not in mempoolA
        assert prouprev2_txid in mempoolA
        # The mempool contains also all the ProReg from the disconnected blocks,
        # except the ones re-registered and replayed on chain B.
        for gm in gmsA:
            assert gm.proTx in mempoolA
        assert rereg_gm.proTx not in mempoolA
        assert replay_gm.proTx not in mempoolA
        assert pre_split_gm1.proTx not in mempoolA
        assert pre_split_gm2.proTx not in mempoolA

        # Mine a block from nodeA so the mempool txes get included
        self.log.info("Mining mempool txes...")
        nodeA.generate(1)
        self.sync_all()
        # mempool_dgm2 and mempool_dgm3 have been included
        gmsB.append(mempool_dgm2)
        gmsB.append(mempool_dgm3)
        # proupserv3 has changed the IP of the pre_split gamemaster 1
        # and proupreg3 has changed its voting address
        gmsB.remove(pre_split_gm1)
        pre_split_gm1.ipport = "127.0.0.1:1001"
        pre_split_gm1.voting = new_voting_address
        gmsB.append(pre_split_gm1)
        # prouprev2 has revoked pre_split gamemaster 2
        gmsB.remove(pre_split_gm2)
        pre_split_gm2.revoked()
        gmsB.append(pre_split_gm2)

        # the ProReg txes, that were added back to the mempool from the
        # disconnected blocks, have been mined again
        gms_all = gmsA + gmsB
        # Check new gm list
        self.check_gm_list_on_node(0, gms_all)
        self.check_gm_list_on_node(1, gms_all)
        self.log.info("Both nodes have %d registered gamemasters." % len(gms_all))

        self.log.info("All good.")


if __name__ == '__main__':
    TiertwoReorgMempoolTest().main()
