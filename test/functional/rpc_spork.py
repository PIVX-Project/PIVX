#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
# -*- coding: utf-8 -*-

from time import sleep

from test_framework.mininode import network_thread_start
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import connect_nodes_bi, p2p_port
from fake_stake.util import TestNode


class PIVX_RPCSporkTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-staking=1']] * self.num_nodes
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')


    def setup_network(self):
        ''' Can't rely on syncing all the nodes when staking=1
        :param:
        :return:
        '''
        self.setup_nodes()
        for i in range(self.num_nodes - 1):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def init_test(self):
        ''' Initializes test parameters
        :param:
        :return:
        '''
        title = "*** Starting %s ***" % self.__class__.__name__
        underline = "-" * len(title)
        self.log.info("\n\n%s\n%s\n%s\n", title, underline, self.description)

        # Setup the p2p connections and start up the network thread.
        self.test_nodes = []
        for i in range(self.num_nodes):
            self.test_nodes.append(TestNode())
            self.test_nodes[i].peer_connect('127.0.0.1', p2p_port(i))

        network_thread_start()  # Start up network handling in another thread
        self.node = self.nodes[0]

        # Let the test nodes get in sync
        for i in range(self.num_nodes):
            self.test_nodes[i].wait_for_verack()


    def printDict(self, d):
        self.log.info("{")
        for k in d:
            self.log.info("  %s = %d" % (k, d[k]))
        self.log.info("}")


    def run_test(self):
        self.description = "Performs tests on the Spork RPC"
        # check spork values:
        sporks = self.nodes[1].spork("show")
        self.printDict(sporks)
        active = self.nodes[1].spork("active")
        assert(not active["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"])
        # activate SPORK 8
        new_value = 1563253447
        res = self.nodes[0].spork("SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT", new_value)
        assert(res == "success")
        sleep(1)
        self.sync_all()
        sporks = self.nodes[1].spork("show")
        self.printDict(sporks)
        assert(sporks["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"] == new_value)
        active = self.nodes[0].spork("active")
        assert (active["SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT"])



if __name__ == '__main__':
    PIVX_RPCSporkTest().main()

