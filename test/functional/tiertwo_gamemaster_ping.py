#!/usr/bin/env python3
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""
Test checking gamemaster ping thread
Does not use functions of HemisTier2TestFramework as we don't want to send
pings on demand. Here, instead, mocktime is disabled, and we just wait with
time.sleep to verify that gamemasters send pings correctly.
"""

import os
import time

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    Decimal,
    p2p_port,
)


class GamemasterPingTest(HemisTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        # 0=miner 1=gm_owner 2=gm_remote
        self.num_nodes = 3


    def run_test(self):
        miner = self.nodes[0]
        owner = self.nodes[1]
        remote = self.nodes[2]
        gmPrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"

        self.log.info("generating 141 blocks...")
        miner.generate(141)
        self.sync_blocks()

        # Create collateral
        self.log.info("funding gamemaster controller...")
        gamemasterAlias = "gmode"
        gmAddress = owner.getnewaddress(gamemasterAlias)
        collateralTxId = miner.sendtoaddress(gmAddress, Decimal('100'))
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
        o = owner.getgamemasteroutputs()
        assert_equal(len(o), 1)
        assert_equal(o[0]["txhash"], collateralTxId)
        vout = o[0]["outputidx"]
        self.log.info("collateral accepted for "+ gamemasterAlias +". Updating gamemaster.conf...")
        confData = gamemasterAlias + " 127.0.0.1:" + str(p2p_port(2)) + " " + \
                   str(gmPrivkey) +  " " + str(collateralTxId) + " " + str(vout)
        destPath = os.path.join(self.options.tmpdir, "node1", "regtest", "gamemaster.conf")
        with open(destPath, "a+", encoding="utf8") as file_object:
            file_object.write("\n")
            file_object.write(confData)

        # Init remote
        self.log.info("initializing remote gamemaster...")
        remote.initgamemaster(gmPrivkey, "127.0.0.1:" + str(p2p_port(2)))

        # sanity check, verify that we are not in IBD
        for i in range(0, len(self.nodes)):
            node = self.nodes[i]
            if (node.getblockchaininfo()['initial_block_downloading']):
                raise AssertionError("Error, node(%s) shouldn't be in IBD." % str(i))

        # Wait until gmsync is complete (max 120 seconds)
        self.log.info("waiting to complete gmsync...")
        start_time = time.time()
        self.wait_until_gmsync_finished()
        self.log.info("GmSync completed in %d seconds" % (time.time() - start_time))
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        # Exercise invalid startgamemaster methods
        self.log.info("exercising invalid startgamemaster methods...")
        assert_raises_rpc_error(-8, "Local start is deprecated.", remote.startgamemaster, "local", False)
        assert_raises_rpc_error(-8, "Many set is deprecated.", owner.startgamemaster, "many", False)
        assert_raises_rpc_error(-8, "Invalid set name", owner.startgamemaster, "foo", False)

        # Send Start message
        self.log.info("sending gamemaster broadcast...")
        self.controller_start_gamemaster(owner, gamemasterAlias)
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        # Wait until gamemaster is enabled everywhere (max 180 secs)
        self.log.info("waiting till gamemaster gets enabled...")
        start_time = time.time()
        time.sleep(5)
        self.wait_until_gm_enabled(collateralTxId, 180)
        self.log.info("Gamemaster enabled in %d seconds" % (time.time() - start_time))
        self.log.info("Good. Gamemaster enabled")
        miner.generate(1)
        self.sync_blocks()
        time.sleep(1)

        last_seen = [self.get_gm_lastseen(node, collateralTxId) for node in self.nodes]
        self.log.info("Current lastseen: %s" % str(last_seen))
        self.log.info("Waiting 2 * 25 seconds and check new lastseen...")
        time.sleep(50)
        new_last_seen = [self.get_gm_lastseen(node, collateralTxId) for node in self.nodes]
        self.log.info("New lastseen: %s" % str(new_last_seen))
        for i in range(self.num_nodes):
            assert_greater_than(new_last_seen[i], last_seen[i])
        self.log.info("All good.")


if __name__ == '__main__':
    GamemasterPingTest().main()
