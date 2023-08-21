#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC calls related to net.

Tests correspond to code in rpc/net.cpp.
"""

from test_framework.messages import NODE_NETWORK
from test_framework.mininode import P2PInterface
from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than_or_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    p2p_port,
    wait_until,
)

class NetTest(HemisTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        self.log.info("Connect nodes both way")
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)

        self._test_connection_count()
        self._test_getnettotals()
        self._test_getnetworkinginfo()
        self._test_getaddednodeinfo()
        # self._test_getpeerinfo()
        self._test_getnodeaddresses()

    def _test_connection_count(self):
        assert_equal(self.nodes[0].getconnectioncount(), 2)

    def _test_getnettotals(self):
        # getnettotals totalbytesrecv and totalbytessent should be
        # consistent with getpeerinfo. Since the RPC calls are not atomic,
        # and messages might have been recvd or sent between RPC calls, call
        # getnettotals before and after and verify that the returned values
        # from getpeerinfo are bounded by those values.
        net_totals_before = self.nodes[0].getnettotals()
        peer_info = self.nodes[0].getpeerinfo()
        net_totals_after = self.nodes[0].getnettotals()
        assert_equal(len(peer_info), 2)
        peers_recv = sum([peer['bytesrecv'] for peer in peer_info])
        peers_sent = sum([peer['bytessent'] for peer in peer_info])

        assert_greater_than_or_equal(peers_recv, net_totals_before['totalbytesrecv'])
        assert_greater_than_or_equal(net_totals_after['totalbytesrecv'], peers_recv)
        assert_greater_than_or_equal(peers_sent, net_totals_before['totalbytessent'])
        assert_greater_than_or_equal(net_totals_after['totalbytessent'], peers_sent)

        # test getnettotals and getpeerinfo by doing a ping
        # the bytes sent/received should change
        # note ping and pong are 32 bytes each
        self.nodes[0].ping()
        wait_until(lambda: (self.nodes[0].getnettotals()['totalbytessent'] >= net_totals_after['totalbytessent'] + 32 * 2), timeout=1)
        wait_until(lambda: (self.nodes[0].getnettotals()['totalbytesrecv'] >= net_totals_after['totalbytesrecv'] + 32 * 2), timeout=1)

    def _test_getnetworkinginfo(self):
        assert_equal(self.nodes[0].getnetworkinfo()['connections'], 2)

        disconnect_nodes(self.nodes[0], 1)
        # Wait a bit for all sockets to close
        wait_until(lambda: self.nodes[0].getnetworkinfo()['connections'] == 0, timeout=3)

        self.log.info("Connect nodes both way")
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 0)
        assert_equal(self.nodes[0].getnetworkinfo()['connections'], 2)

    def _test_getaddednodeinfo(self):
        assert_equal(self.nodes[0].getaddednodeinfo(True), [])
        # add a node (node2) to node0
        ip_port = "127.0.0.1:{}".format(p2p_port(2))
        self.nodes[0].addnode(ip_port, 'add')
        # check that the node has indeed been added
        added_nodes = self.nodes[0].getaddednodeinfo(True, ip_port)
        assert_equal(len(added_nodes), 1)
        assert_equal(added_nodes[0]['addednode'], ip_port)
        # check that a non-existent node returns an error
        assert_raises_rpc_error(-24, "Node has not been added", self.nodes[0].getaddednodeinfo, True, '1.1.1.1')

    def _test_getpeerinfo(self):
        peer_info = [x.getpeerinfo() for x in self.nodes]
        # check both sides of bidirectional connection between nodes
        # the address bound to on one side will be the source address for the other node
        assert_equal(peer_info[0][0]['addrbind'], peer_info[1][0]['addr'])
        assert_equal(peer_info[1][0]['addrbind'], peer_info[0][0]['addr'])

    def _test_getnodeaddresses(self):
        self.nodes[0].add_p2p_connection(P2PInterface())
        services = NODE_NETWORK

        # Add an IPv6 address to the address manager.
        ipv6_addr = "1233:3432:2434:2343:3234:2345:6546:4534"
        self.nodes[0].addpeeraddress(ipv6_addr, 51472)

        # Add 10,000 IPv4 addresses to the address manager. Due to the way bucket
        # and bucket positions are calculated, some of these addresses will collide.
        imported_addrs = []
        for i in range(10000):
            first_octet = i >> 8
            second_octet = i % 256
            a = f"{first_octet}.{second_octet}.1.1"
            imported_addrs.append(a)
            self.nodes[0].addpeeraddress(a, 51472)

        # Fetch the addresses via the RPC and test the results.
        assert_equal(len(self.nodes[0].getnodeaddresses()), 1)  # default count is 1
        assert_equal(len(self.nodes[0].getnodeaddresses(2)), 2)
        assert_equal(len(self.nodes[0].getnodeaddresses(8, "ipv4")), 8)

        # Maximum possible addresses in AddrMan is 10000. The actual number will
        # usually be less due to bucket and bucket position collisions.
        node_addresses = self.nodes[0].getnodeaddresses(0, "ipv4")
        assert_greater_than(len(node_addresses), 5000)
        assert_greater_than(10000, len(node_addresses))
        for a in node_addresses:
            assert_greater_than(a["time"], 1527811200)  # 1st June 2018
            assert_equal(a["services"], services)
            assert a["address"] in imported_addrs
            assert_equal(a["port"], 51472)
            assert_equal(a["network"], "ipv4")

        # Test the IPv6 address.
        res = self.nodes[0].getnodeaddresses(0, "ipv6")
        assert_equal(len(res), 1)
        assert_equal(res[0]["address"], ipv6_addr)
        assert_equal(res[0]["network"], "ipv6")
        assert_equal(res[0]["port"], 51472)
        assert_equal(res[0]["services"], services)

        # Test for the absence of onion addresses.
        for network in ["onion"]:
            assert_equal(self.nodes[0].getnodeaddresses(0, network), [])

        # Test invalid arguments.
        assert_raises_rpc_error(-8, "Address count out of range", self.nodes[0].getnodeaddresses, -1)
        assert_raises_rpc_error(-8, "Network not recognized: Foo", self.nodes[0].getnodeaddresses, 1, "Foo")

if __name__ == '__main__':
    NetTest().main()
