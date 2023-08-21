#!/usr/bin/env python3
# Copyright (c) 2021-2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.
"""Test GM quorum connection flows"""

import time

from random import getrandbits
from test_framework.test_framework import HemisDGMTestFramework
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

class DGMConnectionTest(HemisDGMTestFramework):

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

    def check_peer_info(self, peer_info, gm, is_iqr_conn, inbound=False):
        assert_equal(peer_info["gamemaster"], True)
        assert_equal(peer_info["verif_gm_proreg_tx_hash"], gm.proTx)
        assert_equal(peer_info["verif_gm_operator_pubkey_hash"], bytes_to_hex_str(hash256(bech32_str_to_bytes(gm.operator_pk))))
        assert_equal(peer_info["gamemaster_iqr_conn"], is_iqr_conn)
        # An inbound connection has obviously a different internal port.
        if not inbound:
            assert_equal(peer_info["addr"], gm.ipport)

    def wait_for_peers_info(self, node, quorum_members, is_iqr_conn, inbound=False):
        def find_peer():
            for m in quorum_members:
                for p in node.getpeerinfo():
                    if "verif_gm_proreg_tx_hash" in p and p["verif_gm_proreg_tx_hash"] == m.proTx:
                        self.check_peer_info(p, m, is_iqr_conn, inbound)
                        return True
            return False

        wait_until(find_peer, timeout=120)


    def wait_for_auth_connections(self, node, expected_proreg_txes):
        wait_until(lambda: [pi["verif_gm_proreg_tx_hash"] for pi in node.getpeerinfo()
                            if "verif_gm_proreg_tx_hash" in pi] == expected_proreg_txes, timeout=120)

    def has_single_regular_connection(self, node):
        peer_info = node.getpeerinfo()
        return len(peer_info) == 1 and "verif_gm_proreg_tx_hash" not in peer_info[0]

    def clean_conns_and_disconnect(self, node):
        node.gmconnect("clear_conn")
        self.disconnect_peers(node)
        wait_until(lambda: len(node.getpeerinfo()) == 0, timeout=30)

    def run_test(self):
        self.miner = self.nodes[self.minerPos]

        # initialize and start gamemasters
        self.setup_test()
        assert_equal(len(self.gms), 6)

        ##############################################################
        # 1) Disconnect peers from DGM and add a direct DGM connection
        ##############################################################
        self.log.info("1) Testing single DGM connection, disconnecting nodes..")
        gm1 = self.gms[0]
        gm1_node = self.nodes[gm1.idx]
        self.disconnect_peers(gm1_node)

        self.log.info("disconnected, connecting to a single DGM and auth him..")
        # Now try to connect to the second DGM only
        gm2 = self.gms[1]
        assert gm1_node.gmconnect("single_conn", [gm2.proTx])
        self.wait_for_auth_connections(gm1_node, [gm2.proTx])
        # Check connected peer info: same DGM and gmauth succeeded
        self.wait_for_peers_info(gm1_node, [gm2], is_iqr_conn=False)
        # Same for the the other side
        gm2_node = self.nodes[gm2.idx]
        self.wait_for_peers_info(gm2_node, [gm1], is_iqr_conn=False, inbound=True)
        self.log.info("Completed DGM-to-DGM authenticated connection!")

        ################################################################
        # 2) Disconnect peers from DGM and add quorum members connection
        ################################################################
        self.log.info("2) Testing quorum connections, disconnecting nodes..")
        gm3 = self.gms[2]
        gm4 = self.gms[3]
        gm5 = self.gms[4]
        gm6 = self.gms[5]
        quorum_nodes = [gm3, gm4, gm5, gm6]
        self.disconnect_peers(gm2_node)
        self.wait_for_peers_count([gm2_node], 0)
        self.log.info("disconnected, connecting to quorum members..")
        quorum_members = [gm2.proTx, gm3.proTx, gm4.proTx, gm5.proTx, gm6.proTx]
        assert gm2_node.gmconnect("quorum_members_conn", quorum_members, 1, gm2_node.getbestblockhash())
        # Check connected peer info: same quorum members and gmauth succeeded
        self.wait_for_peers_count([gm2_node], 4)
        self.wait_for_peers_info(gm2_node, quorum_nodes, is_iqr_conn=False)
        # Same for the other side (GMs receiving the new connection)
        for gm_node in [self.nodes[gm3.idx], self.nodes[gm4.idx], self.nodes[gm5.idx], self.nodes[gm6.idx]]:
            self.wait_for_peers_info(gm_node, [gm2], is_iqr_conn=False, inbound=True)
        self.log.info("Completed DGM-to-quorum connections!")

        ##################################################################################
        # 3) Update already connected quorum members in (2) to be intra-quorum connections
        ##################################################################################
        self.log.info("3) Testing connections update to be intra-quorum relay connections")
        assert gm2_node.gmconnect("iqr_members_conn", quorum_members, 1, gm2_node.getbestblockhash())
        time.sleep(2)
        self.wait_for_peers_info(gm2_node, quorum_nodes, is_iqr_conn=True)
        # Same for the other side (GMs receiving the new connection)
        for gm_node in [self.nodes[gm3.idx], self.nodes[gm4.idx], self.nodes[gm5.idx], self.nodes[gm6.idx]]:
            assert gm_node.gmconnect("iqr_members_conn", quorum_members, 1, gm2_node.getbestblockhash())
            self.wait_for_peers_info(gm_node, [gm2], is_iqr_conn=True, inbound=True)
        self.log.info("Completed update to quorum relay members!")

        ###########################################
        # 4) Now test the connections probe process
        ###########################################
        self.log.info("4) Testing GM probe connection process..")
        # Take gm6, disconnect all the nodes and try to probe connection to one of them
        gm6_node = self.nodes[gm6.idx]
        self.disconnect_peers(gm6_node)
        self.log.info("disconnected, probing GM connection..")
        with gm6_node.assert_debug_log(["Gamemaster probe successful for " + gm5.proTx]):
            assert gm_node.gmconnect("probe_conn", [gm5.proTx])
            time.sleep(10) # wait a bit until the connection gets established
        self.log.info("Completed GM connection probe!")

        ###############################################################################
        # 5) Now test regular node disconnecting after receiving an auth DGM connection
        ###############################################################################
        self.log.info("5) Testing regular node disconnection after receiving an auth DGM connection..")
        self.disconnect_peers(self.miner)
        no_version_node = self.miner.add_p2p_connection(TestP2PConn(), send_version=False, wait_for_verack=False)
        self.wait_for_peers_count([self.miner], 1)
        # send the version as it would be a GM
        gm_challenge = getrandbits(256)
        with self.miner.assert_debug_log(["but we're not a gamemaster, disconnecting"]):
            no_version_node.send_message(msg_version(gm_challenge))
            time.sleep(2)
        # as the miner is not a DGM, the miner should had dropped the connection.
        assert_equal(len(self.miner.getpeerinfo()), 0)
        self.log.info("Regular node disconnected auth connection successfully")

        ##############################################################################
        # 6) Now test regular connection refresh after selecting peer as quorum member
        ##############################################################################
        self.log.info("6) Now test regular connection refresh after selecting peer as quorum member..")
        # Cleaning internal data first
        gm5_node = self.nodes[gm5.idx]
        self.clean_conns_and_disconnect(gm5_node)
        self.clean_conns_and_disconnect(gm6_node)

        # Create the regular connection
        connect_nodes(gm5_node, gm6.idx)
        self.wait_for_peers_count([gm5_node], 1)
        assert self.has_single_regular_connection(gm5_node)
        assert self.has_single_regular_connection(gm6_node)

        # Now refresh it to be a quorum member connection
        quorum_hash = gm5_node.getbestblockhash()
        assert gm5_node.gmconnect("quorum_members_conn", [gm6.proTx], 1, quorum_hash)
        assert gm5_node.gmconnect("iqr_members_conn", [gm6.proTx], 1, quorum_hash)
        assert gm6_node.gmconnect("iqr_members_conn", [gm5.proTx], 1, quorum_hash)

        self.wait_for_auth_connections(gm5_node, [gm6.proTx])
        self.log.info("Connection refreshed!")

if __name__ == '__main__':
    DGMConnectionTest().main()


