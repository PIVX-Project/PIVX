#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""Test MN quorum connection flows"""

import time

from random import getrandbits
from test_framework.test_framework import HemisDMNTestFramework
from test_framework.bech32 import bech32_str_to_bytes
from test_framework.mininode import P2PInterface
from test_framework.messages import msg_version
from test_framework.util import (
    assert_equal,
    bytes_to_hex_str,
    connect_nodes,
    hash256,
    wait_until,
)

class TestP2PConn(P2PInterface):
    def on_version(self, message):
        pass

class DMNConnectionTest(HemisDMNTestFramework):

    def set_test_params(self):
        self.set_base_test_params()
        self.extra_args = [["-nuparams=v5_shield:1", "-nuparams=v6_evo:101", "-disabledkg"]] * self.num_nodes
        self.extra_args[0].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

    def disconnect_peers(self, node):
        node.setnetworkactive(False)
        time.sleep(3)
        assert_equal(len(node.getpeerinfo()), 0)
        node.setnetworkactive(True)

    def wait_for_peers_count(self, nodes, expected_count):
        wait_until(lambda: [len(n.getpeerinfo()) for n in nodes] == [expected_count] * len(nodes),
                   timeout=120)

    def check_peer_info(self, peer_info, mn, is_iqr_conn, inbound=False):
        assert_equal(peer_info["masternode"], True)
        assert_equal(peer_info["verif_mn_proreg_tx_hash"], mn.proTx)
        assert_equal(peer_info["verif_mn_operator_pubkey_hash"], bytes_to_hex_str(hash256(bech32_str_to_bytes(mn.operator_pk))))
        assert_equal(peer_info["masternode_iqr_conn"], is_iqr_conn)
        # An inbound connection has obviously a different internal port.
        if not inbound:
            assert_equal(peer_info["addr"], mn.ipport)

    def wait_for_peers_info(self, node, quorum_members, is_iqr_conn, inbound=False):
        def find_peer():
            for m in quorum_members:
                for p in node.getpeerinfo():
                    if "verif_mn_proreg_tx_hash" in p and p["verif_mn_proreg_tx_hash"] == m.proTx:
                        self.check_peer_info(p, m, is_iqr_conn, inbound)
                        return True
            return False

        wait_until(find_peer, timeout=120)


    def wait_for_auth_connections(self, node, expected_proreg_txes):
        wait_until(lambda: [pi["verif_mn_proreg_tx_hash"] for pi in node.getpeerinfo()
                            if "verif_mn_proreg_tx_hash" in pi] == expected_proreg_txes, timeout=120)

    def has_single_regular_connection(self, node):
        peer_info = node.getpeerinfo()
        return len(peer_info) == 1 and "verif_mn_proreg_tx_hash" not in peer_info[0]

    def clean_conns_and_disconnect(self, node):
        node.mnconnect("clear_conn")
        self.disconnect_peers(node)
        wait_until(lambda: len(node.getpeerinfo()) == 0, timeout=30)

    def run_test(self):
        self.miner = self.nodes[self.minerPos]

        # initialize and start masternodes
        self.setup_test()
        assert_equal(len(self.mns), 6)

        ##############################################################
        # 1) Disconnect peers from DMN and add a direct DMN connection
        ##############################################################
        self.log.info("1) Testing single DMN connection, disconnecting nodes..")
        mn1 = self.mns[0]
        mn1_node = self.nodes[mn1.idx]
        self.disconnect_peers(mn1_node)

        self.log.info("disconnected, connecting to a single DMN and auth him..")
        # Now try to connect to the second DMN only
        mn2 = self.mns[1]
        assert mn1_node.mnconnect("single_conn", [mn2.proTx])
        self.wait_for_auth_connections(mn1_node, [mn2.proTx])
        # Check connected peer info: same DMN and mnauth succeeded
        self.wait_for_peers_info(mn1_node, [mn2], is_iqr_conn=False)
        # Same for the the other side
        mn2_node = self.nodes[mn2.idx]
        self.wait_for_peers_info(mn2_node, [mn1], is_iqr_conn=False, inbound=True)
        self.log.info("Completed DMN-to-DMN authenticated connection!")

        ################################################################
        # 2) Disconnect peers from DMN and add quorum members connection
        ################################################################
        self.log.info("2) Testing quorum connections, disconnecting nodes..")
        mn3 = self.mns[2]
        mn4 = self.mns[3]
        mn5 = self.mns[4]
        mn6 = self.mns[5]
        quorum_nodes = [mn3, mn4, mn5, mn6]
        self.disconnect_peers(mn2_node)
        self.wait_for_peers_count([mn2_node], 0)
        self.log.info("disconnected, connecting to quorum members..")
        quorum_members = [mn2.proTx, mn3.proTx, mn4.proTx, mn5.proTx, mn6.proTx]
        assert mn2_node.mnconnect("quorum_members_conn", quorum_members, 1, mn2_node.getbestblockhash())
        # Check connected peer info: same quorum members and mnauth succeeded
        self.wait_for_peers_count([mn2_node], 4)
        self.wait_for_peers_info(mn2_node, quorum_nodes, is_iqr_conn=False)
        # Same for the other side (MNs receiving the new connection)
        for mn_node in [self.nodes[mn3.idx], self.nodes[mn4.idx], self.nodes[mn5.idx], self.nodes[mn6.idx]]:
            self.wait_for_peers_info(mn_node, [mn2], is_iqr_conn=False, inbound=True)
        self.log.info("Completed DMN-to-quorum connections!")

        ##################################################################################
        # 3) Update already connected quorum members in (2) to be intra-quorum connections
        ##################################################################################
        self.log.info("3) Testing connections update to be intra-quorum relay connections")
        assert mn2_node.mnconnect("iqr_members_conn", quorum_members, 1, mn2_node.getbestblockhash())
        time.sleep(2)
        self.wait_for_peers_info(mn2_node, quorum_nodes, is_iqr_conn=True)
        # Same for the other side (MNs receiving the new connection)
        for mn_node in [self.nodes[mn3.idx], self.nodes[mn4.idx], self.nodes[mn5.idx], self.nodes[mn6.idx]]:
            assert mn_node.mnconnect("iqr_members_conn", quorum_members, 1, mn2_node.getbestblockhash())
            self.wait_for_peers_info(mn_node, [mn2], is_iqr_conn=True, inbound=True)
        self.log.info("Completed update to quorum relay members!")

        ###########################################
        # 4) Now test the connections probe process
        ###########################################
        self.log.info("4) Testing MN probe connection process..")
        # Take mn6, disconnect all the nodes and try to probe connection to one of them
        mn6_node = self.nodes[mn6.idx]
        self.disconnect_peers(mn6_node)
        self.log.info("disconnected, probing MN connection..")
        with mn6_node.assert_debug_log(["Masternode probe successful for " + mn5.proTx]):
            assert mn_node.mnconnect("probe_conn", [mn5.proTx])
            time.sleep(10) # wait a bit until the connection gets established
        self.log.info("Completed MN connection probe!")

        ###############################################################################
        # 5) Now test regular node disconnecting after receiving an auth DMN connection
        ###############################################################################
        self.log.info("5) Testing regular node disconnection after receiving an auth DMN connection..")
        self.disconnect_peers(self.miner)
        no_version_node = self.miner.add_p2p_connection(TestP2PConn(), send_version=False, wait_for_verack=False)
        self.wait_for_peers_count([self.miner], 1)
        # send the version as it would be a MN
        mn_challenge = getrandbits(256)
        with self.miner.assert_debug_log(["but we're not a masternode, disconnecting"]):
            no_version_node.send_message(msg_version(mn_challenge))
            time.sleep(2)
        # as the miner is not a DMN, the miner should had dropped the connection.
        assert_equal(len(self.miner.getpeerinfo()), 0)
        self.log.info("Regular node disconnected auth connection successfully")

        ##############################################################################
        # 6) Now test regular connection refresh after selecting peer as quorum member
        ##############################################################################
        self.log.info("6) Now test regular connection refresh after selecting peer as quorum member..")
        # Cleaning internal data first
        mn5_node = self.nodes[mn5.idx]
        self.clean_conns_and_disconnect(mn5_node)
        self.clean_conns_and_disconnect(mn6_node)

        # Create the regular connection
        connect_nodes(mn5_node, mn6.idx)
        self.wait_for_peers_count([mn5_node], 1)
        assert self.has_single_regular_connection(mn5_node)
        assert self.has_single_regular_connection(mn6_node)

        # Now refresh it to be a quorum member connection
        quorum_hash = mn5_node.getbestblockhash()
        assert mn5_node.mnconnect("quorum_members_conn", [mn6.proTx], 1, quorum_hash)
        assert mn5_node.mnconnect("iqr_members_conn", [mn6.proTx], 1, quorum_hash)
        assert mn6_node.mnconnect("iqr_members_conn", [mn5.proTx], 1, quorum_hash)

        self.wait_for_auth_connections(mn5_node, [mn6.proTx])
        self.log.info("Connection refreshed!")

if __name__ == '__main__':
    DMNConnectionTest().main()


