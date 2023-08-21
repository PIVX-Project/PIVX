#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking compatibility code between GM and DGM
"""

from decimal import Decimal

from test_framework.test_framework import HemisTier2TestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
)


class GamemasterCompatibilityTest(HemisTier2TestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 7
        self.enable_mocktime()

        self.minerPos = 0
        self.ownerOnePos = self.ownerTwoPos = 1
        self.remoteOnePos = 2
        self.remoteTwoPos = 3
        self.remoteDGM1Pos = 4
        self.remoteDGM2Pos = 5
        self.remoteDGM3Pos = 6

        self.gamemasterOneAlias = "gmOne"
        self.gamemasterTwoAlias = "gmtwo"

        self.extra_args = [["-nuparams=v5_shield:249", "-nuparams=v6_evo:250", "-whitelist=127.0.0.1"]] * self.num_nodes
        for i in [self.remoteOnePos, self.remoteTwoPos, self.remoteDGM1Pos, self.remoteDGM2Pos, self.remoteDGM3Pos]:
            self.extra_args[i] += ["-listen", "-externalip=127.0.0.1"]
        self.extra_args[self.minerPos].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

        self.gmOnePrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"
        self.gmTwoPrivkey = "92Hkebp3RHdDidGZ7ARgS4orxJAGyFUPDXNqtsYsiwho1HGVRbF"

        self.miner = None
        self.ownerOne = self.ownerTwo = None
        self.remoteOne = None
        self.remoteTwo = None
        self.remoteDGM1 = None
        self.remoteDGM2 = None
        self.remoteDGM3 = None

    def check_gms_status_legacy(self, node, txhash):
        status = node.getgamemasterstatus()
        assert_equal(status["txhash"], txhash)
        assert_equal(status["message"], "Gamemaster successfully started")

    def check_gms_status(self, node, txhash):
        status = node.getgamemasterstatus()
        assert_equal(status["proTxHash"], txhash)
        assert_equal(status["dgmstate"]["PoSePenalty"], 0)
        assert_equal(status["status"], "Ready")

    def check_gm_enabled_count(self, enabled, total):
        for node in self.nodes:
            node_count = node.getgamemastercount()
            assert_equal(node_count['enabled'], enabled)
            assert_equal(node_count['total'], total)

    """
    Checks the block at specified height
    Returns the address of the gm paid (in the coinbase), and the json coinstake tx
    """
    def get_block_gmwinner(self, height):
        blk = self.miner.getblock(self.miner.getblockhash(height), True)
        assert_equal(blk['height'], height)
        cbase_tx = self.miner.getrawtransaction(blk['tx'][0], True)
        assert_equal(len(cbase_tx['vin']), 1)
        cbase_script = height.to_bytes(1 + height // 256, byteorder="little")
        cbase_script = len(cbase_script).to_bytes(1, byteorder="little") + cbase_script + bytearray(1)
        assert_equal(cbase_tx['vin'][0]['coinbase'], cbase_script.hex())
        assert_equal(len(cbase_tx['vout']), 1)
        assert_equal(cbase_tx['vout'][0]['value'], Decimal("3.0"))
        return cbase_tx['vout'][0]['scriptPubKey']['addresses'][0], self.miner.getrawtransaction(blk['tx'][1], True)

    def check_gm_list(self, node, txHashSet):
        # check gamemaster list from node
        gmlist = node.listgamemasters()
        if len(gmlist) != len(txHashSet):
            raise Exception(str(gmlist))
        foundHashes = set([gm["txhash"] for gm in gmlist if gm["txhash"] in txHashSet])
        if len(foundHashes) != len(txHashSet):
            raise Exception(str(gmlist))
        for x in gmlist:
            self.gm_addresses[x["txhash"]] = x["addr"]

    def run_test(self):
        self.gm_addresses = {}
        self.enable_mocktime()
        self.setup_3_gamemasters_network()

        # start with 3 gamemasters (2 legacy + 1 DGM)
        self.check_gm_enabled_count(3, 3)

        # add two more nodes to the network
        self.remoteDGM2 = self.nodes[self.remoteDGM2Pos]
        self.remoteDGM3 = self.nodes[self.remoteDGM3Pos]
        # add more direct connections to the miner
        connect_nodes(self.miner, 2)
        connect_nodes(self.remoteTwo, 0)
        connect_nodes(self.remoteDGM2, 0)
        self.sync_all()

        # check gm list from miner
        txHashSet = set([self.gmOneCollateral.hash, self.gmTwoCollateral.hash, self.proRegTx1])
        self.check_gm_list(self.miner, txHashSet)

        # check status of gamemasters
        self.check_gms_status_legacy(self.remoteOne, self.gmOneCollateral.hash)
        self.log.info("GM1 active. Pays %s" % self.gm_addresses[self.gmOneCollateral.hash])
        self.check_gms_status_legacy(self.remoteTwo, self.gmTwoCollateral.hash)
        self.log.info("GM2 active Pays %s" % self.gm_addresses[self.gmTwoCollateral.hash])
        self.check_gms_status(self.remoteDGM1, self.proRegTx1)
        self.log.info("DGM1 active Pays %s" % self.gm_addresses[self.proRegTx1])

        # Create another DGM, this time without funding the collateral.
        # ProTx references another transaction in the owner's wallet
        self.proRegTx2, self.dgm2Privkey = self.setupDGM(
            self.ownerOne,
            self.miner,
            self.remoteDGM2Pos,
            "internal"
        )
        self.remoteDGM2.initgamemaster(self.dgm2Privkey)

        # check list and status
        self.check_gm_enabled_count(4, 4) # 2 legacy + 2 DGM
        txHashSet.add(self.proRegTx2)
        self.check_gm_list(self.miner, txHashSet)
        self.check_gms_status(self.remoteDGM2, self.proRegTx2)
        self.log.info("DGM2 active Pays %s" % self.gm_addresses[self.proRegTx2])

        # Check block version and coinbase payment
        blk_count = self.miner.getblockcount()
        self.log.info("Checking block version and coinbase payment...")
        payee, cstake_tx = self.get_block_gmwinner(blk_count)
        if payee not in [self.gm_addresses[k] for k in self.gm_addresses]:
            raise Exception("payee %s not found in expected list %s" % (payee, str(self.gm_addresses)))
        assert_equal(len(cstake_tx['vin']), 1)
        assert_equal(len(cstake_tx['vout']), 2)
        assert_equal(cstake_tx['vout'][1]['value'], Decimal("497.0")) # 250 + 250 - 3
        self.log.info("Block at height %d checks out" % blk_count)

        # Now create a DGM, reusing the collateral output of a legacy GM
        self.log.info("Creating a DGM reusing the collateral of a legacy GM...")
        self.proRegTx3, self.dgm3Privkey = self.setupDGM(
            self.ownerOne,
            self.miner,
            self.remoteDGM3Pos,
            "external",
            self.gmOneCollateral,
        )
        # The remote node is shutting down the pinging service
        self.send_3_pings()

        self.remoteDGM3.initgamemaster(self.dgm3Privkey)

        # The legacy gamemaster must no longer be in the list
        # and the DGM must have taken its place
        self.check_gm_enabled_count(4, 4)  # 1 legacy + 3 DGM
        txHashSet.remove(self.gmOneCollateral.hash)
        txHashSet.add(self.proRegTx3)
        for node in self.nodes:
            self.check_gm_list(node, txHashSet)
        self.log.info("Gamemaster list correctly updated by all nodes.")
        self.check_gms_status(self.remoteDGM3, self.proRegTx3)
        self.log.info("DGM3 active Pays %s" % self.gm_addresses[self.proRegTx3])

        # Now try to start a legacy GM with a collateral used by a DGM
        self.log.info("Now trying to start a legacy GM with a collateral of a DGM...")
        self.controller_start_gamemaster(self.ownerOne, self.gamemasterOneAlias)
        self.send_3_pings()

        # the gamemaster list hasn't changed
        self.check_gm_enabled_count(4, 4)
        for node in self.nodes:
            self.check_gm_list(node, txHashSet)
        self.log.info("Gamemaster list correctly unchanged in all nodes.")

        # stake 30 blocks, sync tiertwo data, and check winners
        self.log.info("Staking 30 blocks...")
        self.stake(30, [self.remoteTwo])
        self.sync_blocks()
        self.wait_until_gmsync_finished()

        # check projection
        self.log.info("Checking winners...")
        winners = set([x['winner']['address'] for x in self.miner.getgamemasterwinners()
                       if x['winner']['address'] != "Unknown"])
        # all except gm1 must be scheduled
        gm_addresses = set([self.gm_addresses[k] for k in self.gm_addresses
                            if k != self.gmOneCollateral.hash])
        assert_equal(winners, gm_addresses)

        # check gms paid in the last 20 blocks
        self.log.info("Checking gamemasters paid...")
        blk_count = self.miner.getblockcount()
        gm_payments = {}    # dict address --> payments count
        for i in range(blk_count - 20 + 1, blk_count + 1):
            winner, _ = self.get_block_gmwinner(i)
            if winner not in gm_payments:
                gm_payments[winner] = 0
            gm_payments[winner] += 1
        # two full 10-blocks schedule: all gms must be paid at least twice
        assert_equal(len(gm_payments), len(gm_addresses))
        assert all([x >= 2 for x in gm_payments.values()])
        self.log.info("All good.")



if __name__ == '__main__':
    GamemasterCompatibilityTest().main()
