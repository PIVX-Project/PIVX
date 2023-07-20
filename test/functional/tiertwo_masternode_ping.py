#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking masternode ping thread
Does not use functions of PivxTier2TestFramework as we don't want to send
pings on demand. Here, instead, mocktime is disabled, and we just wait with
time.sleep to verify that masternodes send pings correctly.
"""

import os
import time

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    Decimal,
    p2p_port,
)


class MasternodePingTest(PivxTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        # 0=miner 1=mn_owner 2=mn_remote
        self.num_nodes = 3


    def run_test(self):
        miner = self.nodes[0]
        owner = self.nodes[1]
        remote = self.nodes[2]
        mnPrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"

        self.log.info("generating 141 blocks...")
        miner.generate(141)
        self.sync_blocks()

        # Create collateral
        self.log.info("funding masternode controller...")
        masternodeAlias = "mnode"
        mnAddress = owner.getnewaddress(masternodeAlias)
        collateralTxId = miner.sendtoaddress(mnAddress, Decimal('100'))
        miner.generate(2)
        self.sync_blocks()
        time.sleep(1)
        collateral_rawTx = owner.getrawtransaction(collateralTxId, 1)
        assert_equal(owner.getbalance(), Decimal('100'))
        assert_greater_than(collateral_rawTx["confirmations"], 0)

        # Block time can be up to median time past +1. We might need to wait...
        wait_time = collateral_rawTx["time"] - int(time.time())
        if wait_time > 0:
            self.log.info("Sleep %d seconds to catch up with the chain..." % wait_time)
            time.sleep(wait_time)

        # Setup controller
        self.log.info("controller setup...")
        o = owner.getmasternodeoutputs()
        assert_equal(len(o), 1)
        assert_equal(o[0]["txhash"], collateralTxId)
        vout = o[0]["outputidx"]
        self.log.info("collateral accepted for "+ masternodeAlias +". Updating masternode.conf...")
        confData = masternodeAlias + " 127.0.0.1:" + str(p2p_port(2)) + " " + \
                   str(mnPrivkey) +  " " + str(collateralTxId) + " " + str(vout)
        destPath = os.path.join(self.options.tmpdir, "node1", "regtest", "masternode.conf")
        with open(destPath, "a+", encoding="utf8") as file_object:
            file_object.write("\n")
            file_object.write(confData)

        # Init remote
        self.log.info("initializing remote masternode...")
        remote.initmasternode(mnPrivkey, "127.0.0.1:" + str(p2p_port(2)))

        # sanity check, verify that we are not in IBD
        for i in range(0, len(self.nodes)):
            node = self.nodes[i]
            if (node.getblockchaininfo()['initial_block_downloading']):
                raise AssertionError("Error, node(%s) shouldn't be in IBD." % str(i))

        # Wait until mnsync is complete (max 120 seconds)
        self.log.info("waiting to complete mnsync...")
        start_time = time.time()
        self.wait_until_mnsync_finished()
        self.log.info("MnSync completed in %d seconds" % (time.time() - start_time))
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        # Exercise invalid startmasternode methods
        self.log.info("exercising invalid startmasternode methods...")
        assert_raises_rpc_error(-8, "Local start is deprecated.", remote.startmasternode, "local", False)
        assert_raises_rpc_error(-8, "Many set is deprecated.", owner.startmasternode, "many", False)
        assert_raises_rpc_error(-8, "Invalid set name", owner.startmasternode, "foo", False)

        # Send Start message
        self.log.info("sending masternode broadcast...")
        self.controller_start_masternode(owner, masternodeAlias)
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        # Wait until masternode is enabled everywhere (max 180 secs)
        self.log.info("waiting till masternode gets enabled...")
        start_time = time.time()
        time.sleep(5)
        self.wait_until_mn_enabled(collateralTxId, 180)
        self.log.info("Masternode enabled in %d seconds" % (time.time() - start_time))
        self.log.info("Good. Masternode enabled")
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        last_seen = [self.get_mn_lastseen(node, collateralTxId) for node in self.nodes]
        self.log.info("Current lastseen: %s" % str(last_seen))
        self.log.info("Waiting 2 * 25 seconds and check new lastseen...")
        time.sleep(50)
        new_last_seen = [self.get_mn_lastseen(node, collateralTxId) for node in self.nodes]
        self.log.info("New lastseen: %s" % str(new_last_seen))
        for i in range(self.num_nodes):
            assert_greater_than(new_last_seen[i], last_seen[i])
        self.log.info("All good.")


if __name__ == '__main__':
    MasternodePingTest().main()
