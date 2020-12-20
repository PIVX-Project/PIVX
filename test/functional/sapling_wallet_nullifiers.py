#!/usr/bin/env python3
# Copyright (c) 2016 The Zcash developers
# Copyright (c) 2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import PivxTestFramework
from test_framework.util import *
from decimal import Decimal

def connect_nodes_bi(nodes, a, b):
    connect_nodes(nodes[a], b)
    connect_nodes(nodes[b], a)

class WalletNullifiersTest (PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 4
        saplingUpgrade = ['-nuparams=v5_shield:201']
        self.extra_args = [saplingUpgrade, saplingUpgrade, saplingUpgrade, saplingUpgrade]

    def run_test (self):
        self.nodes[0].generate(1) # activate Sapling

        # add shield addr to node 0
        myzaddr0 = self.nodes[0].getnewshieldaddress()

        # send node 0 taddr to shield addr to get out of coinbase
        # Tests using the default cached chain have one address per coinbase output
        mytaddr = get_coinstake_address(self.nodes[0])
        recipients = []
        recipients.append({"address":myzaddr0, "amount":Decimal('10.0') - Decimal('1')}) # utxo amount less fee

        txid = self.nodes[0].shieldsendmany(mytaddr, recipients)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # add shield addr to node 2
        myzaddr = self.nodes[2].getnewshieldaddress()

        # import node 2 shield addr into node 1
        myzkey = self.nodes[2].exportsaplingkey(myzaddr)
        self.nodes[1].importsaplingkey(myzkey)

        # encrypt node 1 wallet and wait to terminate
        self.nodes[1].node_encrypt_wallet("test")

        # restart node 1
        self.start_node(1, self.extra_args[1])
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 2, 1)
        connect_nodes_bi(self.nodes, 3, 1)
        self.sync_all()

        # send node 0  shield addr to note 2 zaddr
        recipients = []
        recipients.append({"address":myzaddr, "amount":7.0})

        txid = self.nodes[0].shieldsendmany(myzaddr0, recipients)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # check shield addr balance
        zsendmanynotevalue = Decimal('7.0')
        assert_equal(self.nodes[2].getshieldbalance(myzaddr), zsendmanynotevalue)
        assert_equal(self.nodes[1].getshieldbalance(myzaddr), zsendmanynotevalue)

        # add shield addr to node 3
        myzaddr3 = self.nodes[3].getnewshieldaddress()

        # send node 2 shield addr to note 3 shield addr
        recipients = []
        recipients.append({"address":myzaddr3, "amount":2.0})

        txid = self.nodes[2].shieldsendmany(myzaddr, recipients)

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        # check shield addr balance
        zsendmany2notevalue = Decimal('2.0')
        zsendmanyfee = Decimal(self.nodes[2].viewshieldtransaction(txid)['fee'])
        zaddrremaining = zsendmanynotevalue - zsendmany2notevalue - zsendmanyfee
        assert_equal(self.nodes[3].getshieldbalance(myzaddr3), zsendmany2notevalue)
        assert_equal(self.nodes[2].getshieldbalance(myzaddr), zaddrremaining)
        assert_equal(self.nodes[1].getshieldbalance(myzaddr), zaddrremaining)

        # send node 2 shield addr on node 1 to taddr
        # This requires that node 1 be unlocked, which triggers caching of
        # uncached nullifiers.
        self.nodes[1].walletpassphrase("test", 600)
        mytaddr1 = self.nodes[1].getnewaddress()
        recipients = []
        recipients.append({"address":mytaddr1, "amount":1.0})

        txid = self.nodes[1].shieldsendmany(myzaddr, recipients)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # check shield addr balance
        # Now that the encrypted wallet has been unlocked, the note nullifiers
        # have been cached and spent notes can be detected. Thus the two wallets
        # are in agreement once more.
        zsendmany3notevalue = Decimal('1.0')
        zsendmanyfee = Decimal(self.nodes[1].viewshieldtransaction(txid)['fee'])
        zaddrremaining2 = zaddrremaining - zsendmany3notevalue - zsendmanyfee
        assert_equal(self.nodes[1].getshieldbalance(myzaddr), zaddrremaining2)
        assert_equal(self.nodes[2].getshieldbalance(myzaddr), zaddrremaining2)

        # Test viewing keys

        node3mined = Decimal('6250.0')
        assert_equal(self.nodes[3].getshieldbalance(), zsendmany2notevalue)
        assert_equal(self.nodes[3].getbalance(1, False, True, False), node3mined)

        # Add node 1 address and node 2 viewing key to node 3
        myzvkey = self.nodes[2].exportsaplingviewingkey(myzaddr)
        self.nodes[3].importaddress(mytaddr1)
        importvk_result = self.nodes[3].importsaplingviewingkey(myzvkey, 'whenkeyisnew', 1)

        # Check results of importsaplingviewingkey
        assert_equal(importvk_result["address"], myzaddr)

        # Check the address has been imported
        assert_equal(myzaddr in self.nodes[3].listshieldaddresses(), False)
        assert_equal(myzaddr in self.nodes[3].listshieldaddresses(True), True)

        # Node 3 should see the same received notes as node 2; however, there are 2 things:
        # - Some of the notes were change for node 2 but not for node 3.
        # - Each node wallet store transaction time as received. As
        #   `wait_and_assert_operationid_status` is called node 2 and 3 are off by a few seconds.
        # Aside from that the received notes should be the same. So,
        # group by txid and then check that all properties aside from
        # change are equal.
        node2Received = dict([r['txid'], r] for r in self.nodes[2].listreceivedbyshieldaddress(myzaddr))
        node3Received = dict([r['txid'], r] for r in self.nodes[3].listreceivedbyshieldaddress(myzaddr))
        assert_equal(len(node2Received), len(node2Received))
        for txid in node2Received:
            received2 = node2Received[txid]
            received3 = node3Received[txid]
            # the change field will be omitted for received3, but all other fields should be shared
            assert_true(len(received2) >= len(received3))
            for key in received2:
                # check all the properties except for change and blocktime
                if key != 'change' and key != 'blocktime':
                    assert_equal(received2[key], received3[key])

        # Node 3's balances should be unchanged without explicitly requesting
        # to include watch-only balances
        assert_equal(self.nodes[3].getshieldbalance(), zsendmany2notevalue)
        assert_equal(self.nodes[3].getbalance(1, False, True, False), node3mined)

        assert_equal(self.nodes[3].getshieldbalance("*", 1, True), zsendmany2notevalue + zaddrremaining2)
        assert_equal(self.nodes[3].getbalance(1, True, True, False), node3mined + Decimal('1.0'))

        # Check individual balances reflect the above
        assert_equal(self.nodes[3].getreceivedbyaddress(mytaddr1), Decimal('1.0'))
        assert_equal(self.nodes[3].getshieldbalance(myzaddr), Decimal('0.0'))
        assert_equal(self.nodes[3].getshieldbalance(myzaddr, 1, True), zaddrremaining2)

if __name__ == '__main__':
    WalletNullifiersTest().main ()
