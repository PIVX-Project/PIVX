#!/usr/bin/env python3
# Copyright (c) 2021 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test deterministic masternodes conflicts and reorgs.
- Check that in-mempool reuse of mn unique-properties is invalid
- Check mempool eviction after conflict with newly connected block / reorg
- Check deterministic list consensus after reorg
"""

from decimal import Decimal

from test_framework.messages import COutPoint
from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    create_new_dmn,
    connect_nodes,
    disconnect_nodes,
    get_collateral_vout,
)

import random
import time

class TiertwoReorgMempoolTest(PivxTestFramework):

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

    def register_masternode(self, from_node, dmn, collateral_addr):
        dmn.proTx = from_node.protx_register_fund(collateral_addr, dmn.ipport, dmn.owner,
                                                  dmn.operator, dmn.voting, dmn.payee)

    def run_test(self):
        self.disable_mocktime()
        nodeA = self.nodes[0]
        nodeB = self.nodes[1]
        free_idx = 1  # unique id for masternodes. first available.

        # Enforce mn payments and reject legacy mns at block 202
        self.activate_spork(0, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        assert_equal("success", self.set_spork(0, "SPORK_21_LEGACY_MNS_MAX_HEIGHT", 201))
        time.sleep(1)
        assert_equal([201] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_MNS_MAX_HEIGHT")
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

        # Register one masternode before the split
        collateral_addr = nodeA.getnewaddress() # for both collateral and payouts
        pre_split_mn = create_new_dmn(100, nodeA, nodeA.getnewaddress(), None)
        self.register_masternode(nodeA, pre_split_mn, collateral_addr)
        nodeA.generate(1)
        self.sync_blocks()
        mnsA = [pre_split_mn]
        mnsB = [pre_split_mn]
        self.check_mn_list_on_node(0, mnsA)
        self.check_mn_list_on_node(1, mnsB)
        self.log.info("Pre-split masternode registered.")

        # Disconnect the nodes
        self.disconnect_all()   # network splits at block 203

        #
        # -- CHAIN A --
        #

        # Register 5 masternodes, then mine 5 blocks
        self.log.info("Registering masternodes on chain A...")
        for _ in range(5):
            dmn = create_new_dmn(free_idx, nodeA, collateral_addr, None)
            free_idx += 1
            self.register_masternode(nodeA, dmn, collateral_addr)
            mnsA.append(dmn)
        nodeA.generate(5)
        self.check_mn_list_on_node(0, mnsA)
        self.log.info("Masternodes registered on chain A.")

        # Now create a collateral (which will be used later to register a masternode)
        funding_txid = nodeA.sendtoaddress(collateral_addr, Decimal('100'))
        nodeA.generate(1)
        funding_tx_json = nodeA.getrawtransaction(funding_txid, True)
        assert_greater_than(funding_tx_json["confirmations"], 0)
        initial_collateral = COutPoint(int(funding_txid, 16), get_collateral_vout(funding_tx_json))

        # Lock any utxo with less than 106 confs (e.g. change), so we can resurrect everything
        for x in nodeA.listunspent(0, 106):
            nodeA.lockunspent(False, [{"txid": x["txid"], "vout": x["vout"]}])

        # Now send a valid proReg tx to the mempool, without mining it
        mempool_dmn1 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        self.register_masternode(nodeA, mempool_dmn1, collateral_addr)
        assert mempool_dmn1.proTx in nodeA.getrawmempool()

        # Try sending a proReg tx with same owner
        self.log.info("Testing in-mempool duplicate-owner rejection...")
        dmn_A1 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dmn_A1.owner = mempool_dmn1.owner
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_masternode, nodeA, dmn_A1, collateral_addr)
        assert dmn_A1.proTx not in nodeA.getrawmempool()

        # Try sending a proReg tx with same operator
        self.log.info("Testing in-mempool duplicate-operator rejection...")
        dmn_A2 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dmn_A2.operator = mempool_dmn1.operator
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_masternode, nodeA, dmn_A2, collateral_addr)
        assert dmn_A2.proTx not in nodeA.getrawmempool()

        # Try sending a proReg tx with same IP
        self.log.info("Testing proReg in-mempool duplicate-IP rejection...")
        dmn_A3 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        dmn_A3.ipport = mempool_dmn1.ipport
        assert_raises_rpc_error(-26, "protx-dup",
                                self.register_masternode, nodeA, dmn_A3, collateral_addr)
        assert dmn_A3.proTx not in nodeA.getrawmempool()

        # Now send other 2 valid proReg tx to the mempool, without mining them
        self.log.info("Sending more ProReg txes to the mempool...")
        mempool_dmn2 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        mempool_dmn3 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        free_idx += 1
        self.register_masternode(nodeA, mempool_dmn2, collateral_addr)
        self.register_masternode(nodeA, mempool_dmn3, collateral_addr)

        # Send to the mempool a ProRegTx using the collateral mined after the split
        mempool_dmn4 = create_new_dmn(free_idx, nodeA, collateral_addr, None)
        mempool_dmn4.collateral = initial_collateral
        self.protx_register_ext(nodeA, nodeA, mempool_dmn4, mempool_dmn4.collateral, True)

        # Now send a valid proUpServ tx to the mempool, without mining it
        proupserv1_txid = nodeA.protx_update_service(pre_split_mn.proTx, "127.0.0.1:1000")

        # Try sending another update, reusing the same ip of the previous mempool tx
        self.log.info("Testing proUpServ in-mempool duplicate-IP rejection...")
        assert_raises_rpc_error(-26, "protx-dup", nodeA.protx_update_service, mnsA[0].proTx, "127.0.0.1:1000")

        # Now send other two valid proUpServ txes to the mempool, without mining them
        proupserv2_txid = nodeA.protx_update_service(mnsA[2].proTx, "127.0.0.1:2000")
        proupserv3_txid = nodeA.protx_update_service(pre_split_mn.proTx, "127.0.0.1:1001")

        # Now nodeA has 4 proReg txes in its mempool, and 3 proUpServ txes
        mempoolA = nodeA.getrawmempool()
        assert mempool_dmn1.proTx in mempoolA
        assert mempool_dmn2.proTx in mempoolA
        assert mempool_dmn3.proTx in mempoolA
        assert mempool_dmn4.proTx in mempoolA
        assert proupserv1_txid in mempoolA
        assert proupserv2_txid in mempoolA
        assert proupserv3_txid in mempoolA

        assert_equal(nodeA.getblockcount(), 208)

        #
        # -- CHAIN B --
        #
        collateral_addr = nodeB.getnewaddress()
        self.log.info("Registering masternodes on chain B...")

        # Register first the 3 nodes that conflict with the mempool of nodes[0]
        # mine one block after each registration
        for dmn in [dmn_A1, dmn_A2, dmn_A3]:
            self.register_masternode(nodeB, dmn, collateral_addr)
            mnsB.append(dmn)
            nodeB.generate(1)
        self.check_mn_list_on_node(1, mnsB)

        # Pick the proReg for the first MN registered on chain A, and replay it on chain B
        self.log.info("Replaying a masternode on a different chain...")
        mnsA.remove(pre_split_mn)
        replay_mn = mnsA.pop(0)
        mnsB.append(replay_mn)  # same proTx hash
        nodeB.sendrawtransaction(nodeA.getrawtransaction(replay_mn.proTx, False))
        nodeB.generate(1)
        self.check_mn_list_on_node(1, mnsB)

        # Now pick a proReg for another MN registered on chain A, and re-register it on chain B
        self.log.info("Re-registering a masternode on a different chain...")
        rereg_mn = random.choice(mnsA)
        mnsA.remove(rereg_mn)
        self.register_masternode(nodeB, rereg_mn, collateral_addr)
        mnsB.append(rereg_mn)   # changed proTx hash
        nodeB.generate(1)
        self.check_mn_list_on_node(1, mnsB)

        # Register 5 more masternodes. One per block.
        for _ in range(5):
            dmn = create_new_dmn(free_idx, nodeB, collateral_addr, None)
            free_idx += 1
            self.register_masternode(nodeB, dmn, collateral_addr)
            mnsB.append(dmn)
            nodeB.generate(1)

        # Register one masternode reusing the IP of the proUpServ mempool tx on chainA
        dmn1000 = create_new_dmn(0, nodeB, collateral_addr, None)
        dmn1000.ipport = "127.0.0.1:1000"
        mnsB.append(dmn1000)
        self.register_masternode(nodeB, dmn1000, collateral_addr)

        # Then mine 10 more blocks on chain B
        nodeB.generate(10)
        self.check_mn_list_on_node(1, mnsB)
        self.log.info("Masternodes registered on chain B.")

        assert_equal(nodeB.getblockcount(), 222)

        #
        # -- RECONNECT --
        #

        # Reconnect and sync (give it some more time)
        self.log.info("Reconnecting nodes...")
        self.connect_all()
        self.sync_blocks(wait=3, timeout=180)

        # Both nodes have the same list (mnB)
        self.log.info("Checking masternode list...")
        self.check_mn_list_on_node(0, mnsB)
        self.check_mn_list_on_node(1, mnsB)

        self.log.info("Checking mempool...")
        mempoolA = nodeA.getrawmempool()
        # The first mempool proReg tx has been removed from nodeA's mempool due to
        # conflicts with the masternodes of chain B, now connected.
        # The fourth mempool proReg tx has been removed because the collateral it
        # was referencing has been disconnected.
        assert mempool_dmn1.proTx not in mempoolA
        assert mempool_dmn2.proTx in mempoolA
        assert mempool_dmn3.proTx in mempoolA
        assert mempool_dmn4.proTx not in mempoolA
        # The first mempool proUpServ tx has been removed as the IP (port=1000) is
        # now used by a newly connected masternode.
        # The second mempool proUpServ tx has been removed as it was meant to update
        # a masternode that is not in the deterministic list anymore.
        assert proupserv1_txid not in mempoolA
        assert proupserv2_txid not in mempoolA
        assert proupserv3_txid in mempoolA
        # The mempool contains also all the ProReg from the disconnected blocks,
        # except the ones re-registered and replayed on chain B.
        for mn in mnsA:
            assert mn.proTx in mempoolA
        assert rereg_mn.proTx not in mempoolA
        assert replay_mn.proTx not in mempoolA
        assert pre_split_mn.proTx not in mempoolA

        # Mine a block from nodeA so the mempool proReg txes get included
        self.log.info("Mining mempool txes...")
        nodeA.generate(1)
        self.sync_all()
        # mempool_dmn2 and mempool_dmn3 have been included
        mnsB.append(mempool_dmn2)
        mnsB.append(mempool_dmn3)
        # proupserv3 has changed the IP of the pre_split masternode
        mnsB.remove(pre_split_mn)
        pre_split_mn.ipport = "127.0.0.1:1001"
        mnsB.append(pre_split_mn)
        # the ProReg txes, that were added back to the mempool from the
        # disconnected blocks, have been mined again
        mns_all = mnsA + mnsB
        # Check new mn list
        self.check_mn_list_on_node(0, mns_all)
        self.check_mn_list_on_node(1, mns_all)
        self.log.info("Both nodes have %d registered masternodes." % len(mns_all))

        self.log.info("All good.")


if __name__ == '__main__':
    TiertwoReorgMempoolTest().main()
