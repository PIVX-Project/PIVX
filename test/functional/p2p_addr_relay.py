#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test addr relay
"""

from test_framework.messages import (
    CAddress,
    NODE_NETWORK,
    msg_addr,
)
from test_framework.mininode import (
    P2PInterface,
    mininode_lock
)
from test_framework.test_framework import HemisTestFramework
from test_framework.util import assert_equal

import random
import time

ADDRS = []
for i in range(10):
    addr = CAddress()
    addr.time = int(time.time()) + i
    addr.nServices = NODE_NETWORK
    addr.ip = "123.123.123.{}".format(i % 256)
    addr.port = 8333 + i
    ADDRS.append(addr)


class AddrReceiver(P2PInterface):
    _tokens = 10
    def on_addr(self, message):
        for addr in message.addrs:
            assert_equal(addr.nServices, 1)
            assert addr.ip.startswith('123.123.123.')
            assert (8333 <= addr.port < 8343)

    def on_getaddr(self, message):
        # When the node sends us a getaddr, it increments the addr relay tokens for the connection by 1000
        self._tokens += 1000

    @property
    def tokens(self):
        with mininode_lock:
            return self._tokens

    def increment_tokens(self, n):
        # When we move mocktime forward, the node increments the addr relay tokens for its peers
        with mininode_lock:
            self._tokens += n


class AddrTest(HemisTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = False
        self.num_nodes = 1
        self.extra_args = [["-whitelist=127.0.0.1"]]

    def run_test(self):
        self.log.info('Create connection that sends addr messages')
        addr_source = self.nodes[0].add_p2p_connection(P2PInterface())
        msg = msg_addr()

        self.log.info('Send too large addr message')
        msg.addrs = ADDRS * 101
        with self.nodes[0].assert_debug_log(['addr message size = 1010']):
            addr_source.send_and_ping(msg)

        self.log.info('Check that addr message content is relayed and added to addrman')
        addr_receiver = self.nodes[0].add_p2p_connection(AddrReceiver())
        msg.addrs = ADDRS
        self.mocktime += 10
        self.nodes[0].setmocktime(self.mocktime)
        with self.nodes[0].assert_debug_log([
                'Added 10 addresses from 127.0.0.1: 0 tried',
                'received: addr (301 bytes) peer=0',
                'sending addr (301 bytes) peer=1',
        ]):
            addr_source.send_and_ping(msg)
            self.nodes[0].setmocktime(int(time.time()) + 30 * 60)
            addr_receiver.sync_with_ping()
            self.rate_limit_tests()

    def setup_rand_addr_msg(self, num):
        addrs = []
        for i in range(num):
            addr = CAddress()
            addr.time = self.mocktime + i
            addr.nServices = NODE_NETWORK
            addr.ip = f"{random.randrange(128,169)}.{random.randrange(1,255)}.{random.randrange(1,255)}.{random.randrange(1,255)}"
            addr.port = 8333
            addrs.append(addr)
        msg = msg_addr()
        msg.addrs = addrs
        return msg

    def send_addrs_and_test_rate_limiting(self, peer, new_addrs, total_addrs):
        """Send an addr message and check that the number of addresses processed and rate-limited is as expected"""

        peer.send_and_ping(self.setup_rand_addr_msg(new_addrs))

        peerinfo = self.nodes[0].getpeerinfo()[2]
        addrs_processed = peerinfo['addr_processed']
        addrs_rate_limited = peerinfo['addr_rate_limited']
        self.log.info(f'addrs_processed = {addrs_processed}, addrs_rate_limited = {addrs_rate_limited}')
        self.log.info(f'peer_tokens = {peer.tokens}')

        assert_equal(addrs_processed, min(total_addrs, peer.tokens))
        assert_equal(addrs_rate_limited, max(0, total_addrs - peer.tokens))

    def rate_limit_tests(self):

        self.mocktime = int(time.time())
        self.nodes[0].setmocktime(self.mocktime)

        peer = self.nodes[0].add_p2p_connection(AddrReceiver())

        self.log.info(f'Test rate limiting of addr processing for inbound peers')

        # Send 600 addresses.
        self.send_addrs_and_test_rate_limiting(peer, 600, 600)

        # Send 400 more addresses.
        self.send_addrs_and_test_rate_limiting(peer, 400, 1000)

        # Send 10 more. As we reached the processing limit for nodes, no more addresses should be procesesd.
        self.send_addrs_and_test_rate_limiting(peer, 10, 1010)

        # Advance the time by 100 seconds, permitting the processing of 10 more addresses.
        # Send 200 and verify that 10 are processed.
        self.mocktime += 100
        self.nodes[0].setmocktime(self.mocktime)
        peer.increment_tokens(10)

        self.send_addrs_and_test_rate_limiting(peer, 200, 1210)

        # Advance the time by 1000 seconds, permitting the processing of 100 more addresses.
        # Send 200 and verify that 100 are processed.
        self.mocktime += 1000
        self.nodes[0].setmocktime(self.mocktime)
        peer.increment_tokens(100)

        self.send_addrs_and_test_rate_limiting(peer, 200, 1410)

        self.nodes[0].disconnect_p2ps()



if __name__ == '__main__':
    AddrTest().main()
