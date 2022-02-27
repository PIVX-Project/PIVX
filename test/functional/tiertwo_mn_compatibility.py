#!/usr/bin/env python3
# Copyright (c) 2021 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking compatibility code between MN and DMN
"""

from decimal import Decimal

from test_framework.test_framework import PivxTier2TestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
)


class MasternodeCompatibilityTest(PivxTier2TestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 7
        self.enable_mocktime()

        self.minerPos = 0
        self.ownerOnePos = self.ownerTwoPos = 1
        self.remoteOnePos = 2
        self.remoteTwoPos = 3
        self.remoteDMN1Pos = 4
        self.remoteDMN2Pos = 5
        self.remoteDMN3Pos = 6

        self.masternodeOneAlias = "mnOne"
        self.masternodeTwoAlias = "mntwo"

        self.extra_args = [["-nuparams=v5_shield:249", "-nuparams=v6_evo:250", "-whitelist=127.0.0.1"]] * self.num_nodes
        for i in [self.remoteOnePos, self.remoteTwoPos, self.remoteDMN1Pos, self.remoteDMN2Pos, self.remoteDMN3Pos]:
            self.extra_args[i] += ["-listen", "-externalip=127.0.0.1"]
        self.extra_args[self.minerPos].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

        self.mnOnePrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"
        self.mnTwoPrivkey = "92Hkebp3RHdDidGZ7ARgS4orxJAGyFUPDXNqtsYsiwho1HGVRbF"

        self.miner = None
        self.ownerOne = self.ownerTwo = None
        self.remoteOne = None
        self.remoteTwo = None
        self.remoteDMN1 = None
        self.remoteDMN2 = None
        self.remoteDMN3 = None

    def check_mns_status_legacy(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["txhash"], txhash)
        assert_equal(status["message"], "Masternode successfully started")

    def check_mns_status(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["proTxHash"], txhash)
        assert_equal(status["dmnstate"]["PoSePenalty"], 0)
        assert_equal(status["status"], "Ready")

    def check_mn_enabled_count(self, enabled, total):
        for node in self.nodes:
            node_count = node.getmasternodecount()
            assert_equal(node_count['enabled'], enabled)
            assert_equal(node_count['total'], total)

    """
    Checks the block at specified height
    Returns the address of the mn paid (in the coinbase), and the json coinstake tx
    """
    def get_block_mnwinner(self, height):
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

    def check_mn_list(self, node, txHashSet):
        # check masternode list from node
        mnlist = node.listmasternodes()
        if len(mnlist) != len(txHashSet):
            raise Exception(str(mnlist))
        foundHashes = set([mn["txhash"] for mn in mnlist if mn["txhash"] in txHashSet])
        if len(foundHashes) != len(txHashSet):
            raise Exception(str(mnlist))
        for x in mnlist:
            self.mn_addresses[x["txhash"]] = x["addr"]

    def run_test(self):
        self.mn_addresses = {}
        self.enable_mocktime()
        self.setup_masternodes_network()

        # start with 3 masternodes (2 legacy + 1 DMN)
        self.check_mn_enabled_count(3, 3)

        # add two more nodes to the network
        self.remoteDMN2 = self.nodes[self.remoteDMN2Pos]
        self.remoteDMN3 = self.nodes[self.remoteDMN3Pos]
        # add more direct connections to the miner
        connect_nodes(self.miner, 2)
        connect_nodes(self.remoteTwo, 0)
        connect_nodes(self.remoteDMN2, 0)
        self.sync_all()

        # check mn list from miner
        txHashSet = set([self.mnOneCollateral.hash, self.mnTwoCollateral.hash, self.proRegTx1])
        self.check_mn_list(self.miner, txHashSet)

        # check status of masternodes
        self.check_mns_status_legacy(self.remoteOne, self.mnOneCollateral.hash)
        self.log.info("MN1 active. Pays %s" % self.mn_addresses[self.mnOneCollateral.hash])
        self.check_mns_status_legacy(self.remoteTwo, self.mnTwoCollateral.hash)
        self.log.info("MN2 active Pays %s" % self.mn_addresses[self.mnTwoCollateral.hash])
        self.check_mns_status(self.remoteDMN1, self.proRegTx1)
        self.log.info("DMN1 active Pays %s" % self.mn_addresses[self.proRegTx1])

        # Create another DMN, this time without funding the collateral.
        # ProTx references another transaction in the owner's wallet
        self.proRegTx2, self.dmn2Privkey = self.setupDMN(
            self.ownerOne,
            self.miner,
            self.remoteDMN2Pos,
            "internal"
        )
        self.remoteDMN2.initmasternode(self.dmn2Privkey)

        # check list and status
        self.check_mn_enabled_count(4, 4) # 2 legacy + 2 DMN
        txHashSet.add(self.proRegTx2)
        self.check_mn_list(self.miner, txHashSet)
        self.check_mns_status(self.remoteDMN2, self.proRegTx2)
        self.log.info("DMN2 active Pays %s" % self.mn_addresses[self.proRegTx2])

        # Check block version and coinbase payment
        blk_count = self.miner.getblockcount()
        self.log.info("Checking block version and coinbase payment...")
        payee, cstake_tx = self.get_block_mnwinner(blk_count)
        if payee not in [self.mn_addresses[k] for k in self.mn_addresses]:
            raise Exception("payee %s not found in expected list %s" % (payee, str(self.mn_addresses)))
        assert_equal(len(cstake_tx['vin']), 1)
        assert_equal(len(cstake_tx['vout']), 2)
        assert_equal(cstake_tx['vout'][1]['value'], Decimal("497.0")) # 250 + 250 - 3
        self.log.info("Block at height %d checks out" % blk_count)

        # Now create a DMN, reusing the collateral output of a legacy MN
        self.log.info("Creating a DMN reusing the collateral of a legacy MN...")
        self.proRegTx3, self.dmn3Privkey = self.setupDMN(
            self.ownerOne,
            self.miner,
            self.remoteDMN3Pos,
            "external",
            self.mnOneCollateral,
        )
        # The remote node is shutting down the pinging service
        self.send_3_pings()

        self.remoteDMN3.initmasternode(self.dmn3Privkey)

        # The legacy masternode must no longer be in the list
        # and the DMN must have taken its place
        self.check_mn_enabled_count(4, 4)  # 1 legacy + 3 DMN
        txHashSet.remove(self.mnOneCollateral.hash)
        txHashSet.add(self.proRegTx3)
        for node in self.nodes:
            self.check_mn_list(node, txHashSet)
        self.log.info("Masternode list correctly updated by all nodes.")
        self.check_mns_status(self.remoteDMN3, self.proRegTx3)
        self.log.info("DMN3 active Pays %s" % self.mn_addresses[self.proRegTx3])

        # Now try to start a legacy MN with a collateral used by a DMN
        self.log.info("Now trying to start a legacy MN with a collateral of a DMN...")
        self.controller_start_masternode(self.ownerOne, self.masternodeOneAlias)
        self.send_3_pings()

        # the masternode list hasn't changed
        self.check_mn_enabled_count(4, 4)
        for node in self.nodes:
            self.check_mn_list(node, txHashSet)
        self.log.info("Masternode list correctly unchanged in all nodes.")

        # stake 30 blocks, sync tiertwo data, and check winners
        self.log.info("Staking 30 blocks...")
        self.stake(30, [self.remoteTwo])
        self.sync_blocks()
        self.wait_until_mnsync_finished()

        # check projection
        self.log.info("Checking winners...")
        winners = set([x['winner']['address'] for x in self.miner.getmasternodewinners()
                       if x['winner']['address'] != "Unknown"])
        # all except mn1 must be scheduled
        mn_addresses = set([self.mn_addresses[k] for k in self.mn_addresses
                            if k != self.mnOneCollateral.hash])
        assert_equal(winners, mn_addresses)

        # check mns paid in the last 20 blocks
        self.log.info("Checking masternodes paid...")
        blk_count = self.miner.getblockcount()
        mn_payments = {}    # dict address --> payments count
        for i in range(blk_count - 20 + 1, blk_count + 1):
            winner, _ = self.get_block_mnwinner(i)
            if winner not in mn_payments:
                mn_payments[winner] = 0
            mn_payments[winner] += 1
        # two full 10-blocks schedule: all mns must be paid at least twice
        assert_equal(len(mn_payments), len(mn_addresses))
        assert all([x >= 2 for x in mn_payments.values()])
        self.log.info("All good.")



if __name__ == '__main__':
    MasternodeCompatibilityTest().main()
