#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from time import sleep

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    satoshi_round,
    get_coinstake_address,
    wait_until,
)


# Test wallet behaviour with Sapling addresses
class WalletSaplingTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 4
        saplingUpgrade = ['-nuparams=v5_shield:201']
        self.extra_args = [saplingUpgrade, saplingUpgrade, saplingUpgrade, saplingUpgrade]
        self.extra_args[0].append('-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi')

    def check_tx_in_mempool(self, txids):
        self.sync_mempools()
        mempool = self.nodes[0].getrawmempool(True)
        for txid in txids:
            assert txid in mempool

    def wait_for_spork(self, fEnabled, spork_id):
        sleep(2)
        for i in range(self.num_nodes):
            wait_until(lambda: self.is_spork_active(i, spork_id) == fEnabled, timeout=5)

    def run_test(self):
        self.nodes[0].generate(2)
        self.sync_all()
        assert_equal(self.nodes[1].getblockcount(), 202)
        taddr1 = self.nodes[1].getnewaddress()
        saplingAddr0 = self.nodes[0].getnewshieldaddress()
        saplingAddr1 = self.nodes[1].getnewshieldaddress()

        # Verify addresses
        assert(saplingAddr0 in self.nodes[0].listshieldaddresses())
        assert(saplingAddr1 in self.nodes[1].listshieldaddresses())

        # Verify balance
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('0'))
        assert_equal(self.nodes[1].getshieldbalance(saplingAddr1), Decimal('0'))
        assert_equal(self.nodes[1].getreceivedbyaddress(taddr1), Decimal('0'))

        recipients = [{"address": saplingAddr0, "amount": Decimal('10')}]

        # Try fee too low
        fee_too_low = 0.001
        self.log.info("Trying to send a transaction with fee too low...")
        assert_raises_rpc_error(-4, "Fee set (%.3f) too low. Must be at least" % fee_too_low,
                                self.nodes[0].rawshieldsendmany,
                                "from_transparent", recipients, 1, fee_too_low)

        # Try fee too high.
        fee_too_high = 20
        self.log.info("Good. It was not possible. Now try a tx with fee too high...")
        assert_raises_rpc_error(-4, "The transaction fee is too high: %.2f >" % fee_too_high,
                                self.nodes[0].rawshieldsendmany,
                                "from_transparent", recipients, 1, fee_too_high)

        # Trying to send a rawtx with low fee directly
        self.log.info("Good. It was not possible. Now try with a raw tx...")
        self.restart_node(0, extra_args=self.extra_args[0]+['-minrelaytxfee=0.0000001'])
        rawtx_hex = self.nodes[0].rawshieldsendmany("from_transparent", recipients, 1)
        self.restart_node(0, extra_args=self.extra_args[0])
        connect_nodes(self.nodes[0], 1)
        assert_raises_rpc_error(-26, "insufficient fee",
                                self.nodes[0].sendrawtransaction, rawtx_hex)
        self.log.info("Good. Not accepted in the mempool.")

        # Fixed fee
        fee = 0.05

        # Node 0 shields some funds
        # taddr -> Sapling
        self.log.info("TX 1: shield funds from specified transparent address.")
        mytxid1 = self.nodes[0].shieldsendmany(get_coinstake_address(self.nodes[0]), recipients, 1, fee)

        # shield more funds automatically selecting the transparent inputs
        self.log.info("TX 2: shield funds from any transparent address.")
        mytxid2 = self.nodes[0].shieldsendmany("from_transparent", recipients, 1, fee)
        self.check_tx_in_mempool([mytxid1, mytxid2])

        self.nodes[2].generate(1)
        self.sync_all()

        # shield more funds creating and then sending a raw transaction
        self.log.info("TX 3: shield funds creating and sending raw transaction.")
        tx_hex = self.nodes[0].rawshieldsendmany("from_transparent", recipients, 1, fee)

        # Check SPORK_20 for sapling maintenance mode
        SPORK_20 = "SPORK_20_SAPLING_MAINTENANCE"
        self.activate_spork(0, SPORK_20)
        self.wait_for_spork(True, SPORK_20)
        assert_raises_rpc_error(-26, "bad-tx-sapling-maintenance",
                                self.nodes[0].sendrawtransaction, tx_hex)
        self.log.info("Good. Not accepted when SPORK_20 is active.")

        # Try with RPC...
        assert_raises_rpc_error(-8, "SHIELD in maintenance (SPORK 20)",
                                self.nodes[0].shieldsendmany, "from_transparent", recipients, 1, fee)

        # Disable SPORK_20 and retry
        sleep(5)
        self.deactivate_spork(0, SPORK_20)
        self.wait_for_spork(False, SPORK_20)
        mytxid3 = self.nodes[0].sendrawtransaction(tx_hex)
        self.log.info("Good. Accepted when SPORK_20 is not active.")
        self.check_tx_in_mempool([mytxid3])

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('30'))
        assert_equal(self.nodes[1].getshieldbalance(saplingAddr1), Decimal('0'))
        assert_equal(self.nodes[1].getreceivedbyaddress(taddr1), Decimal('0'))
        self.log.info("Balances check out")

        # Now disconnect the block, activate SPORK_20, and try to reconnect it
        disconnect_nodes(self.nodes[0], 1)
        tip_hash = self.nodes[0].getbestblockhash()
        self.nodes[0].invalidateblock(tip_hash)
        assert tip_hash != self.nodes[0].getbestblockhash()
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('20'))
        self.log.info("Now trying to connect block with shield tx, when SPORK_20 is active")
        self.activate_spork(0, SPORK_20)
        self.nodes[0].reconsiderblock(tip_hash)
        assert tip_hash != self.nodes[0].getbestblockhash()         # Block NOT connected
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('20'))
        self.log.info("Good. Not possible.")

        # Deactivate SPORK_20 and reconnect
        sleep(1)
        self.deactivate_spork(0, SPORK_20)
        self.nodes[0].reconsiderblock(tip_hash)
        self.nodes[0].syncwithvalidationinterfacequeue()
        assert_equal(tip_hash, self.nodes[0].getbestblockhash())    # Block connected
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('30'))
        self.log.info("Reconnected after deactivation of SPORK_20. Balance restored.")
        connect_nodes(self.nodes[0], 1)

        # Node 0 sends some shield funds to node 1
        # Sapling -> Sapling
        #         -> Sapling (change)
        self.log.info("TX 4: shield transaction from specified sapling address.")
        recipients4 = [{"address": saplingAddr1, "amount": Decimal('10')}]
        mytxid4 = self.nodes[0].shieldsendmany(saplingAddr0, recipients4, 1, fee)
        self.check_tx_in_mempool([mytxid4])

        self.nodes[2].generate(1)
        self.sync_all()

        # Send more shield funds (this time with automatic selection of the source)
        self.log.info("TX 5: shield transaction from any sapling address.")
        recipients5 = [{"address": saplingAddr1, "amount": Decimal('5')}]
        mytxid5 = self.nodes[0].shieldsendmany("from_shield", recipients5, 1, fee)
        self.check_tx_in_mempool([mytxid5])

        self.nodes[2].generate(1)
        self.sync_all()

        # Send more shield funds (with create + send raw transaction)
        self.log.info("TX 6: shield raw transaction.")
        tx_hex = self.nodes[0].rawshieldsendmany("from_shield", recipients5, 1, fee)
        mytxid6 = self.nodes[0].sendrawtransaction(tx_hex)
        self.check_tx_in_mempool([mytxid6])

        self.nodes[2].generate(1)
        self.sync_all()

        # Shield more funds to a different address to verify multi-source notes spending
        saplingAddr2 = self.nodes[0].getnewshieldaddress()
        self.log.info("TX 7: shield funds to later verify multi source notes spending.")
        recipients = [{"address": saplingAddr2, "amount": Decimal('10')}]
        mytxid7 = self.nodes[0].shieldsendmany(get_coinstake_address(self.nodes[0]), recipients, 1, fee)
        self.check_tx_in_mempool([mytxid7])

        self.nodes[2].generate(5)
        self.sync_all()

        # Verify multi-source notes spending
        tAddr0 = self.nodes[0].getnewaddress()
        self.log.info("TX 8: verifying multi source notes spending.")
        recipients = [{"address": tAddr0, "amount": Decimal('11')}]
        mytxid8 = self.nodes[0].shieldsendmany("from_shield", recipients, 1, fee)
        self.check_tx_in_mempool([mytxid8])

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('4.9'))   # 30 received - (20 sent + 0.15 fee) - 4.95 sent
        assert_equal(self.nodes[1].getshieldbalance(saplingAddr1), Decimal('20'))    # 20 received
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr2), Decimal('3.9'))   # 10 received - 10 sent + 3.9 change
        assert_equal(self.nodes[1].getreceivedbyaddress(taddr1), Decimal('0'))
        assert_equal(self.nodes[0].getshieldbalance(), Decimal('8.8'))
        self.log.info("Balances check out")

        # Node 1 sends some shield funds to node 0, as well as unshielding
        # Sapling -> Sapling
        #         -> taddr
        #         -> Sapling (change)
        self.log.info("TX 10: deshield funds from specified sapling address.")
        recipients7 = [{"address": saplingAddr0, "amount": Decimal('8')}]
        recipients7.append({"address": taddr1, "amount": Decimal('10')})
        mytxid7 = self.nodes[1].shieldsendmany(saplingAddr1, recipients7, 1, fee)
        self.check_tx_in_mempool([mytxid7])

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('12.9')) # 4.9 prev balance + 8 received
        assert_equal(self.nodes[1].getshieldbalance(saplingAddr1), Decimal('1.95')) # 20 prev balance - (18 sent + 0.05 fee)
        assert_equal(self.nodes[1].getreceivedbyaddress(taddr1), Decimal('10'))
        self.log.info("Balances check out")

        # Verify existence of Sapling related JSON fields
        resp = self.nodes[0].getrawtransaction(mytxid7, 1)
        assert_equal(Decimal(resp['valueBalance']), Decimal('10.05'))    # 20 shield input - 8 shield spend - 1.95 change
        assert_equal(len(resp['vShieldSpend']), 3)
        assert_equal(len(resp['vShieldOutput']), 2)
        assert('bindingSig' in resp)
        shieldedSpend = resp['vShieldSpend'][0]
        assert('cv' in shieldedSpend)
        assert('anchor' in shieldedSpend)
        assert('nullifier' in shieldedSpend)
        assert('rk' in shieldedSpend)
        assert('proof' in shieldedSpend)
        assert('spendAuthSig' in shieldedSpend)
        shieldedOutput = resp['vShieldOutput'][0]
        assert('cv' in shieldedOutput)
        assert('cmu' in shieldedOutput)
        assert('ephemeralKey' in shieldedOutput)
        assert('encCiphertext' in shieldedOutput)
        assert('outCiphertext' in shieldedOutput)
        assert('proof' in shieldedOutput)
        self.log.info("Raw transaction decoding checks out")

        # Verify importing a spending key will update the nullifiers and witnesses correctly
        self.log.info("Checking exporting/importing a spending key...")
        sk0 = self.nodes[0].exportsaplingkey(saplingAddr0)
        saplingAddrInfo0 = self.nodes[2].importsaplingkey(sk0, "yes")
        assert_equal(saplingAddrInfo0["address"], saplingAddr0)
        assert_equal(self.nodes[2].getshieldbalance(saplingAddrInfo0["address"]), Decimal('12.9'))
        sk1 = self.nodes[1].exportsaplingkey(saplingAddr1)
        saplingAddrInfo1 = self.nodes[2].importsaplingkey(sk1, "yes")
        assert_equal(saplingAddrInfo1["address"], saplingAddr1)
        assert_equal(self.nodes[2].getshieldbalance(saplingAddrInfo1["address"]), Decimal('1.95'))

        # Verify importing a viewing key will update the nullifiers and witnesses correctly
        self.log.info("Checking exporting/importing a viewing key...")
        extfvk0 = self.nodes[0].exportsaplingviewingkey(saplingAddr0)
        saplingAddrInfo0 = self.nodes[3].importsaplingviewingkey(extfvk0, "yes")
        assert_equal(saplingAddrInfo0["address"], saplingAddr0)
        assert_equal(Decimal(self.nodes[3].getshieldbalance(saplingAddrInfo0["address"], 1, True)), Decimal('12.9'))
        extfvk1 = self.nodes[1].exportsaplingviewingkey(saplingAddr1)
        saplingAddrInfo1 = self.nodes[3].importsaplingviewingkey(extfvk1, "yes")
        assert_equal(saplingAddrInfo1["address"], saplingAddr1)
        assert_equal(self.nodes[3].getshieldbalance(saplingAddrInfo1["address"], 1, True), Decimal('1.95'))
        # no balance in the wallet
        assert_equal(self.nodes[3].getshieldbalance(), Decimal('0'))
        # watch only balance
        assert_equal(self.nodes[3].getshieldbalance("*", 1, True), Decimal('14.85'))

        # Now shield some funds using sendmany
        self.log.info("TX11: Shielding coins to multiple destinations with sendmany RPC...")
        prev_balance = self.nodes[0].getbalance()
        recipients8 = {saplingAddr0: Decimal('8'), saplingAddr1: Decimal('1'), saplingAddr2: Decimal('0.5')}
        mytxid11 = self.nodes[0].sendmany("", recipients8)
        self.check_tx_in_mempool([mytxid11])
        self.log.info("Done. Checking details and balances...")

        # Decrypted transaction details should be correct
        pt = self.nodes[0].viewshieldtransaction(mytxid11)
        fee = pt["fee"]
        assert_equal(pt['txid'], mytxid11)
        assert_equal(len(pt['spends']), 0)
        assert_equal(len(pt['outputs']), 3)
        found = [False] * 3
        for out in pt['outputs']:
            assert_equal(pt['outputs'].index(out), out['output'])
            if out['address'] == saplingAddr0:
                assert_equal(out['outgoing'], False)
                assert_equal(out['value'], Decimal('8'))
                found[0] = True
            elif out['address'] == saplingAddr1:
                assert_equal(out['outgoing'], True)
                assert_equal(out['value'], Decimal('1'))
                found[1] = True
            else:
                assert_equal(out['address'], saplingAddr2)
                assert_equal(out['outgoing'], False)
                assert_equal(out['value'], Decimal('0.5'))
                found[2] = True
        assert_equal(found, [True] * 3)

        # Verify balance
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('20.9'))  # 12.9 prev balance + 8 received
        assert_equal(self.nodes[1].getshieldbalance(saplingAddr1), Decimal('2.95'))  # 1.95 prev balance + 1 received
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr2), Decimal('4.4'))  # 3.9 prev balance + 0.5 received
        # Balance of node 0 is: prev_balance - 1 PIV (+fee) sent externally +  250 PIV matured coinbase
        assert_equal(self.nodes[0].getbalance(), satoshi_round(prev_balance + Decimal('249') - Decimal(fee)))

        # Now shield some funds using sendtoaddress
        self.log.info("TX12: Shielding coins with sendtoaddress RPC...")
        prev_balance = self.nodes[0].getbalance()
        mytxid12 = self.nodes[0].sendtoaddress(saplingAddr0, Decimal('10'))
        self.check_tx_in_mempool([mytxid12])
        self.log.info("Done. Checking details and balances...")

        # Decrypted transaction details should be correct
        pt = self.nodes[0].viewshieldtransaction(mytxid12)
        fee = pt["fee"]
        assert_equal(pt['txid'], mytxid12)
        assert_equal(len(pt['spends']), 0)
        assert_equal(len(pt['outputs']), 1)
        out = pt['outputs'][0]
        assert_equal(out['address'], saplingAddr0)
        assert_equal(out['outgoing'], False)
        assert_equal(out['value'], Decimal('10'))

        # Verify balance
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getshieldbalance(saplingAddr0), Decimal('30.9'))  # 20.9 prev balance + 10 received

        self.log.info("All good.")


if __name__ == '__main__':
    WalletSaplingTest().main()
