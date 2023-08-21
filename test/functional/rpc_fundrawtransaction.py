#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import HemisTestFramework
from test_framework.util import (
    assert_fee_amount,
    assert_equal,
    assert_raises_rpc_error,
    assert_greater_than,
    assert_greater_than_or_equal,
    count_bytes,
    find_vout_for_address,
    Decimal,
    DecimalAmt,
)


def get_unspent(listunspent, amount):
    for utx in listunspent:
        if utx['amount'] == amount:
            return utx
    raise AssertionError('Could not find unspent with amount={}'.format(amount))

def check_outputs(outputs, dec_tx):
    for out in outputs:
        found = False
        for x in dec_tx['vout']:
            if x['scriptPubKey']['addresses'][0] == out and \
                    DecimalAmt(float(x['value'])) == DecimalAmt(outputs[out]):
                found = True
                break
        if not found:
            return False
    return True


class RawTransactionsTest(HemisTestFramework):

    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

    def create_and_fund(self, nodeid, inputs, outputs):
        rawtx = self.nodes[nodeid].createrawtransaction(inputs, outputs)
        rawtxfund = self.nodes[nodeid].fundrawtransaction(rawtx)
        for vin in self.nodes[nodeid].decoderawtransaction(rawtxfund['hex'])['vin']:
            assert_equal(vin['scriptSig']['hex'], '')  # not signed
        rawtxsigned = self.nodes[nodeid].signrawtransaction(rawtxfund['hex'])
        assert rawtxsigned['complete']
        return self.nodes[nodeid].decoderawtransaction(rawtxsigned['hex']), rawtxfund['fee'], rawtxfund['changepos']

    def get_tot_balance(self, nodeid):
        wi = self.nodes[nodeid].getwalletinfo()
        return wi['balance'] + wi['immature_balance']

    def run_test(self):
        self.min_relay_tx_fee = self.nodes[0].getnetworkinfo()['relayfee']
        # This test is not meant to test fee estimation and we'd like
        # to be sure all txs are sent at a consistent desired feerate
        for node in self.nodes:
            node.settxfee(float(self.min_relay_tx_fee))

        # if the fee's positive delta is higher than this value tests will fail,
        # neg. delta always fail the tests.
        # The size of the signature of every input may be at most 2 bytes larger
        # than a minimum sized signature.

        #            = 2 bytes * minRelayTxFeePerByte
        self.fee_tolerance = 2 * self.min_relay_tx_fee/1000

        print("Mining blocks...")
        self.nodes[1].generate(1)
        self.sync_all()
        self.nodes[0].generate(121)
        self.sync_all()

        # ensure that setting changePosition in fundraw with an exact match is handled properly
        rawmatch = self.nodes[1].createrawtransaction([], {self.nodes[2].getnewaddress(): DecimalAmt(250.0)})
        rawmatch = self.nodes[1].fundrawtransaction(rawmatch, {"changePosition": 1, "subtractFeeFromOutputs": [0]})
        assert_equal(rawmatch["changepos"], -1)

        watchonly_address = self.nodes[0].getnewaddress()
        watchonly_pubkey = self.nodes[0].validateaddress(watchonly_address)["pubkey"]
        self.watchonly_amount = DecimalAmt(200.0)
        self.nodes[3].importpubkey(watchonly_pubkey, "", True)
        self.watchonly_txid = self.nodes[0].sendtoaddress(watchonly_address, float(self.watchonly_amount))

        # Lock UTXO so nodes[0] doesn't accidentally spend it
        self.watchonly_vout = find_vout_for_address(self.nodes[0], self.watchonly_txid, watchonly_address)
        self.nodes[0].lockunspent(False, True, [{"txid": self.watchonly_txid, "vout": self.watchonly_vout}])

        self.nodes[0].sendtoaddress(self.nodes[3].getnewaddress(), float(self.watchonly_amount) / 10)

        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1.5)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1.0)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 5.0)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        self.test_simple()
        self.test_simple_two_coins()
        self.test_simple_one_coin()
        self.test_simple_two_outputs()
        self.test_change()
        self.test_no_change()
        self.test_change_position()
        self.test_invalid_option()
        self.test_invalid_change_address()
        self.test_valid_change_address()
        self.test_coin_selection()
        self.test_two_vin()
        self.test_two_vin_two_vout()
        self.test_invalid_input()
        self.test_fee_p2pkh()
        self.test_fee_p2pkh_multi_out()
        self.test_fee_p2sh()
        self.test_fee_4of5()
        self.test_spend_2of2()
        self.test_locked_wallet()
        self.test_many_inputs_fee()
        self.test_many_inputs()
        self.test_op_return()
        self.test_watchonly()
        self.test_all_watched_funds()
        self.test_option_feerate()
        self.test_option_subtract_fee_from_outputs()

    def test_simple(self):
        self.log.info("simple test")
        dec_tx, fee, changepos = self.create_and_fund(2, [], {self.nodes[0].getnewaddress(): 1.0})
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_greater_than(changepos, -1)                      # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(0.5))

    def test_simple_two_coins(self):
        self.log.info("simple test with two coins")
        dec_tx, fee, changepos = self.create_and_fund(2, [], {self.nodes[0].getnewaddress(): 2.2})
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_greater_than(changepos, -1)                      # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(0.3))

    def test_simple_one_coin(self):
        self.log.info("simple test with one coin")
        dec_tx, fee, changepos = self.create_and_fund(2, [], {self.nodes[0].getnewaddress(): 2.6})
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_greater_than(changepos, -1)                      # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(2.4))

    def test_simple_two_outputs(self):
        self.log.info("simple test with two outputs")
        outputs = {self.nodes[0].getnewaddress(): 2.6, self.nodes[1].getnewaddress(): 2.5}
        dec_tx, fee, changepos = self.create_and_fund(2, [], outputs)
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_greater_than(changepos, -1)                      # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(0.9))
        assert check_outputs(outputs, dec_tx)                   # check outputs

    def test_change(self):
        self.log.info("test with a vin > required amount")
        utx = get_unspent(self.nodes[2].listunspent(), 5)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        dec_tx, fee, changepos = self.create_and_fund(2, inputs, outputs)
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_greater_than(changepos, -1)                      # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(4.0))
        assert check_outputs(outputs, dec_tx)                   # check outputs
        self.test_no_change_fee = fee                           # Use the same fee for the next tx

    def test_no_change(self):
        self.log.info("test with no change output")
        utx = get_unspent(self.nodes[2].listunspent(), 5)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 5.0 - float(self.test_no_change_fee) - float(self.fee_tolerance)}
        dec_tx, fee, changepos = self.create_and_fund(2, inputs, outputs)
        assert_equal(len(dec_tx['vin']) > 0, True)              # test if we have enought inputs
        assert_equal(changepos, -1)                             # check (no) change
        assert check_outputs(outputs, dec_tx)                   # check outputs

    def test_change_position(self):
        """Ensure setting changePosition in fundraw with an exact match is handled properly."""
        self.log.info("test not-used changePosition option")
        utx = get_unspent(self.nodes[2].listunspent(), 5)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 5.0 - float(self.test_no_change_fee) - float(self.fee_tolerance)}
        rawtx = self.nodes[2].createrawtransaction(inputs, outputs)
        rawtxfund = self.nodes[2].fundrawtransaction(rawtx, {"changePosition": 0})
        assert_equal(rawtxfund["changepos"], -1)

    def test_invalid_option(self):
        self.log.info("test with an invalid option")
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        rawtx = self.nodes[2].createrawtransaction([], outputs)
        assert_raises_rpc_error(-3, "Unexpected key foo",
                                self.nodes[2].fundrawtransaction, rawtx, {'foo': 'bar'})

    def test_invalid_change_address(self):
        self.log.info("test with an invalid change address")
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        rawtx = self.nodes[2].createrawtransaction([], outputs)
        assert_raises_rpc_error(-8, "changeAddress must be a valid hemis address",
                                self.nodes[2].fundrawtransaction, rawtx, {'changeAddress': 'foobar'})

    def test_valid_change_address(self):
        self.log.info("test with a provided change address")
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        rawtx = self.nodes[2].createrawtransaction([], outputs)
        change = self.nodes[2].getnewaddress()
        assert_raises_rpc_error(-8, "changePosition out of bounds",
                                self.nodes[2].fundrawtransaction, rawtx, {'changeAddress': change,
                                                                          'changePosition': 2})
        rawtxfund = self.nodes[2].fundrawtransaction(rawtx, {'changeAddress': change, 'changePosition': 0})
        dec_tx = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        out = dec_tx['vout'][0]
        assert_equal(change, out['scriptPubKey']['addresses'][0])

    def test_coin_selection(self):
        self.log.info("test with a vin < required amount")
        utx = get_unspent(self.nodes[2].listunspent(), 1)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        rawtx = self.nodes[2].createrawtransaction(inputs, outputs)

        # 4-byte version + 1-byte vin count + 36-byte prevout then script_len
        rawtx = rawtx[:82] + "0100" + rawtx[84:]

        dec_tx = self.nodes[2].decoderawtransaction(rawtx)
        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])
        assert_equal("00", dec_tx['vin'][0]['scriptSig']['hex'])

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        changepos = rawtxfund['changepos']
        dec_tx = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        assert_equal(len(dec_tx['vin']) > 0, True)                  # test if we have enought inputs
        assert_equal("00", dec_tx['vin'][0]['scriptSig']['hex'])    # check vin sig again
        assert_equal(len(dec_tx['vout']), 2)
        assert_greater_than(changepos, -1)                          # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(1.5))
        assert check_outputs(outputs, dec_tx)                       # check outputs

    def test_two_vin(self):
        self.log.info("test with 2 vins")
        utx = get_unspent(self.nodes[2].listunspent(), 1)
        utx2 = get_unspent(self.nodes[2].listunspent(), 5)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}, {'txid': utx2['txid'], 'vout': utx2['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 6.0}
        dec_tx, fee, changepos = self.create_and_fund(2, inputs, outputs)
        assert_equal(len(dec_tx['vin']) > 0, True)                  # test if we have enought inputs
        assert check_outputs(outputs, dec_tx)                       # check outputs
        assert_equal(len(dec_tx['vout']), 2)
        matchingIns = len([x for x in dec_tx['vin'] if x['txid'] in [y['txid'] for y in inputs]])
        assert_equal(matchingIns, 2)                                # match 2 vins given as params
        assert_greater_than(changepos, -1)                          # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(1.5))

    def test_two_vin_two_vout(self):
        self.log.info("test with 2 vins and 2 vouts")
        utx = get_unspent(self.nodes[2].listunspent(), 1)
        utx2 = get_unspent(self.nodes[2].listunspent(), 5)
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}, {'txid': utx2['txid'], 'vout': utx2['vout']}]
        outputs = {self.nodes[0].getnewaddress(): 6.0, self.nodes[0].getnewaddress(): 1.0}
        dec_tx, fee, changepos = self.create_and_fund(2, inputs, outputs)
        assert_equal(len(dec_tx['vin']) > 0, True)                  # test if we have enought inputs
        assert check_outputs(outputs, dec_tx)                       # check outputs
        assert_equal(len(dec_tx['vout']), 3)
        matchingIns = len([x for x in dec_tx['vin'] if x['txid'] in [y['txid'] for y in inputs]])
        assert_equal(matchingIns, 2)                                # match 2 vins given as params
        assert_greater_than(changepos, -1)                          # check change
        assert_equal(Decimal(dec_tx['vout'][changepos]['value']) + fee, DecimalAmt(0.5))

    def test_invalid_input(self):
        self.log.info("test with invalid vin")
        inputs = [{'txid': "1c7f966dab21119bac53213a2bc7532bff1fa844c124fd750a7d0b1332440bd1", 'vout': 0}] #invalid vin!
        outputs = {self.nodes[0].getnewaddress(): 1.0}
        rawtx = self.nodes[2].createrawtransaction(inputs, outputs)
        assert_raises_rpc_error(-32603, "Insufficient funds", self.nodes[2].fundrawtransaction, rawtx)

    def test_fee_p2pkh(self):
        """Compare fee of a standard pubkeyhash transaction."""
        self.log.info("test p2pkh fee")
        inputs = []
        outputs = {self.nodes[1].getnewaddress(): 1.1}
        dec_tx, fee, changepos = self.create_and_fund(0, inputs, outputs)

        #create same transaction over sendtoaddress
        txId = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 1.1)
        signedFee = self.nodes[0].getrawmempool(True)[txId]['fee']

        #compare fee
        feeDelta = Decimal(fee) - Decimal(signedFee)
        assert feeDelta >= 0 and feeDelta <= self.fee_tolerance

    def test_fee_p2pkh_multi_out(self):
        """Compare fee of a standard pubkeyhash transaction with multiple outputs."""
        self.log.info("test p2pkh fee with multiple outputs")
        outputs = {self.nodes[1].getnewaddress(): 1.1,
                   self.nodes[1].getnewaddress(): 1.2,
                   self.nodes[1].getnewaddress(): 0.1,
                   self.nodes[1].getnewaddress(): 1.3,
                   self.nodes[1].getnewaddress(): 0.2,
                   self.nodes[1].getnewaddress(): 0.3}
        dec_tx, fee, changepos = self.create_and_fund(0, [], outputs)

        #create same transaction over sendtoaddress
        txId = self.nodes[0].sendmany("", outputs)
        signedFee = self.nodes[0].getrawmempool(True)[txId]['fee']

        #compare fee
        feeDelta = Decimal(fee) - Decimal(signedFee)
        assert feeDelta >= 0 and feeDelta <= self.fee_tolerance

    def test_fee_p2sh(self):
        """Compare fee of a 2-of-2 multisig p2sh transaction."""
        self.log.info("test 2-of-2 multisig p2sh fee")
        addrs = [self.nodes[1].getnewaddress() for _ in range(2)]
        mSigAddr = self.nodes[1].addmultisigaddress(2, addrs)
        outputs = {mSigAddr: 1.1}
        dec_tx, fee, changepos = self.create_and_fund(0, [], outputs)

        # Create same transaction over sendtoaddress.
        txId = self.nodes[0].sendtoaddress(mSigAddr, 1.1)
        signedFee = self.nodes[0].getrawmempool(True)[txId]['fee']

        # Compare fee.
        feeDelta = Decimal(fee) - Decimal(signedFee)
        assert feeDelta >= 0 and feeDelta <= self.fee_tolerance

    def test_fee_4of5(self):
        """Compare fee of a a 4-of-5 multisig p2sh transaction."""
        self.log.info("test 4-of-5 addresses multisig fee")
        addrs = [self.nodes[1].getnewaddress() for _ in range(5)]
        mSigAddr = self.nodes[1].addmultisigaddress(4, addrs)
        outputs = {mSigAddr: 1.1}
        dec_tx, fee, changepos = self.create_and_fund(0, [], outputs)

        # Create same transaction over sendtoaddress.
        txId = self.nodes[0].sendtoaddress(mSigAddr, 1.1)
        signedFee = self.nodes[0].getrawmempool(True)[txId]['fee']

        # Compare fee.
        feeDelta = Decimal(fee) - Decimal(signedFee)
        assert feeDelta >= 0 and feeDelta <= self.fee_tolerance

    def test_spend_2of2(self):
        """Spend a 2-of-2 multisig transaction over fundraw."""
        self.log.info("test spending from 2-of-2 multisig")
        addrs = [self.nodes[2].getnewaddress() for _ in range(2)]
        mSigAddr = self.nodes[2].addmultisigaddress(2, addrs)

        # Send 50.1 HMS to mSigAddr.
        self.nodes[0].sendtoaddress(mSigAddr, 50.1)
        self.nodes[0].generate(1)
        self.sync_all()

        # fund transaction with msigAddr fund
        oldBalance = self.nodes[1].getbalance()
        outputs = {self.nodes[1].getnewaddress(): 50.0}
        dec_tx, fee, changepos = self.create_and_fund(2, [], outputs)
        self.nodes[2].sendrawtransaction(dec_tx['hex'])
        self.nodes[2].generate(1)
        self.sync_all()

        # Make sure funds are received at node1.
        assert_equal(oldBalance + DecimalAmt(50.0), self.nodes[1].getbalance())

    def test_locked_wallet(self):
        self.log.info("test locked wallet")

        self.nodes[1].encryptwallet("test")

        # Drain the keypool.
        self.nodes[1].getnewaddress()
        self.nodes[1].getrawchangeaddress()
        utx = get_unspent(self.nodes[1].listunspent(), DecimalAmt(250.0))
        inputs = [{'txid': utx['txid'], 'vout': utx['vout']}]
        outputs = {self.nodes[0].getnewaddress(): round(250.0 - float(self.test_no_change_fee) - float(self.fee_tolerance), 8)}
        rawtx = self.nodes[1].createrawtransaction(inputs, outputs)
        # fund a transaction that does not require a new key for the change output
        self.nodes[1].fundrawtransaction(rawtx)

        # fund a transaction that requires a new key for the change output
        # creating the key must be impossible because the wallet is locked
        outputs = {self.nodes[0].getnewaddress(): 250.0}
        rawtx = self.nodes[1].createrawtransaction([], outputs)
        assert_raises_rpc_error(-32603, "Can't generate a change-address key. Please call keypoolrefill first.", self.nodes[1].fundrawtransaction, rawtx)

        # Refill the keypool.
        self.nodes[1].walletpassphrase("test", 100)
        self.nodes[1].keypoolrefill(8) #need to refill the keypool to get an internal change address
        self.nodes[1].walletlock()

        assert_raises_rpc_error(-13, "walletpassphrase", self.nodes[1].sendtoaddress, self.nodes[0].getnewaddress(), 1.2)

        oldBalance = self.get_tot_balance(0)
        rawtx = self.nodes[1].createrawtransaction([], outputs)
        fundedTx = self.nodes[1].fundrawtransaction(rawtx)

        # Now we need to unlock.
        self.nodes[1].walletpassphrase("test", 600)
        signedTx = self.nodes[1].signrawtransaction(fundedTx['hex'])
        self.nodes[1].sendrawtransaction(signedTx['hex'])
        self.nodes[1].generate(1)
        self.sync_all()

        # Make sure funds are received at node0.
        assert_equal(oldBalance + DecimalAmt(250.0), self.get_tot_balance(0))

    def test_many_inputs_int(self, fCompareFee):
        """Multiple (~19) inputs tx test | Compare fee."""
        info = "compare fee" if fCompareFee else "send"
        self.log.info("test with many inputs (%s)" % info)

        # Empty node1, send some small coins from node0 to node1.
        self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), round((float(self.nodes[1].getbalance()) - 0.001), 8))
        self.nodes[1].generate(1)
        self.sync_all()

        for i in range(0,20):
            self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.01)
        self.nodes[0].generate(1)
        self.sync_all()

        # Fund a tx with ~20 small inputs.
        outputs = {self.nodes[0].getnewaddress():0.15, self.nodes[0].getnewaddress(): 0.04}
        rawtx = self.nodes[1].createrawtransaction([], outputs)
        fundedTx = self.nodes[1].fundrawtransaction(rawtx)

        if fCompareFee:
            # Create same transaction over sendtoaddress.
            txId = self.nodes[1].sendmany("", outputs)
            signedFee = self.nodes[1].getrawmempool(True)[txId]['fee']

            # Compare fee.
            feeDelta = Decimal(fundedTx['fee']) - Decimal(signedFee)
            assert feeDelta >= 0 and feeDelta <= self.fee_tolerance * 19  #~19 inputs

        else:
            # Sign and send the rawtransaction
            oldBalance = self.get_tot_balance(0)
            fundedAndSignedTx = self.nodes[1].signrawtransaction(fundedTx['hex'])
            self.nodes[1].sendrawtransaction(fundedAndSignedTx['hex'])
            self.nodes[1].generate(1)
            self.sync_all()
            assert_equal(oldBalance + DecimalAmt(0.19), self.get_tot_balance(0))

    def test_many_inputs_fee(self):
        return self.test_many_inputs_int(True)

    def test_many_inputs(self):
        return self.test_many_inputs_int(False)

    def test_op_return(self):
        self.log.info("test fundrawtxn with OP_RETURN and no vin")
        rawtx = "0100000000010000000000000000066a047465737400000000"

        dec_tx = self.nodes[2].decoderawtransaction(rawtx)

        assert_equal(len(dec_tx['vin']), 0)
        assert_equal(len(dec_tx['vout']), 1)

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        dec_tx = self.nodes[2].decoderawtransaction(rawtxfund['hex'])

        assert_greater_than(len(dec_tx['vin']), 0)      # at least one vin
        assert_equal(len(dec_tx['vout']), 2)            # one change output added

    def test_watchonly(self):
        self.log.info("test using only watchonly")

        outputs = {self.nodes[2].getnewaddress(): float(self.watchonly_amount) / 2}
        rawtx = self.nodes[3].createrawtransaction([], outputs)

        result = self.nodes[3].fundrawtransaction(rawtx, {"includeWatching": True})
        res_dec = self.nodes[0].decoderawtransaction(result["hex"])
        assert_equal(len(res_dec["vin"]), 1)
        assert_equal(res_dec["vin"][0]["txid"], self.watchonly_txid)

        assert "fee" in result.keys()
        assert_greater_than(result["changepos"], -1)

    def test_all_watched_funds(self):
        self.log.info("test using entirety of watched funds")

        outputs = {self.nodes[2].getnewaddress(): float(self.watchonly_amount)}
        rawtx = self.nodes[3].createrawtransaction([], outputs)

        result = self.nodes[3].fundrawtransaction(rawtx, {"includeWatching": True})
        res_dec = self.nodes[0].decoderawtransaction(result["hex"])
        assert_equal(len(res_dec["vin"]), 2)
        assert res_dec["vin"][0]["txid"] == self.watchonly_txid or res_dec["vin"][1]["txid"] == self.watchonly_txid

        assert_greater_than(result["fee"], 0)
        assert_greater_than(result["changepos"], -1)
        assert_equal(result["fee"] + res_dec["vout"][result["changepos"]]["value"], float(self.watchonly_amount) / 10)

        signedtx = self.nodes[3].signrawtransaction(result["hex"])
        assert not signedtx["complete"]
        signedtx = self.nodes[0].signrawtransaction(signedtx["hex"])
        assert signedtx["complete"]
        self.nodes[0].sendrawtransaction(signedtx["hex"])
        self.nodes[0].generate(1)
        self.sync_all()

    def test_option_feerate(self):
        self.log.info("test feeRate option")
        # Make sure there is exactly one input so coin selection can't skew the result.
        assert_equal(len(self.nodes[1].listunspent()), 1)
        rawtx = self.nodes[1].createrawtransaction([], {self.nodes[1].getnewaddress(): 0.001})
        result = self.nodes[1].fundrawtransaction(rawtx)  # uses self.min_relay_tx_fee (set by settxfee)
        result2 = self.nodes[1].fundrawtransaction(rawtx, {"feeRate": 2 * float(self.min_relay_tx_fee)})
        result3 = self.nodes[1].fundrawtransaction(rawtx, {"feeRate": 10 * float(self.min_relay_tx_fee)})
        result_fee_rate = result['fee'] * 1000 / count_bytes(result['hex'])
        assert_fee_amount(result2['fee'], count_bytes(result2['hex']), 2 * result_fee_rate)
        assert_fee_amount(result3['fee'], count_bytes(result3['hex']), 10 * result_fee_rate)

    def test_option_subtract_fee_from_outputs(self):
        self.log.info("Test fundrawtxn subtractFeeFromOutputs option")

        # Make sure there is exactly one input so coin selection can't skew the result.
        assert_equal(len(self.nodes[3].listunspent(1)), 1)

        inputs = []
        outputs = {self.nodes[2].getnewaddress(): 1}
        rawtx = self.nodes[3].createrawtransaction(inputs, outputs)

        # Test subtract fee from outputs with feeRate
        result = [self.nodes[3].fundrawtransaction(rawtx),  # uses self.min_relay_tx_fee (set by settxfee)
            self.nodes[3].fundrawtransaction(rawtx, {"subtractFeeFromOutputs": []}),  # empty subtraction list
            self.nodes[3].fundrawtransaction(rawtx, {"subtractFeeFromOutputs": [0]}),  # uses self.min_relay_tx_fee (set by settxfee)
            self.nodes[3].fundrawtransaction(rawtx, {"feeRate": 2 * self.min_relay_tx_fee}),
            self.nodes[3].fundrawtransaction(rawtx, {"feeRate": 2 * self.min_relay_tx_fee, "subtractFeeFromOutputs": [0]}),]
        dec_tx = [self.nodes[3].decoderawtransaction(tx_['hex']) for tx_ in result]
        output = [d['vout'][1 - r['changepos']]['value'] for d, r in zip(dec_tx, result)]
        change = [d['vout'][r['changepos']]['value'] for d, r in zip(dec_tx, result)]

        assert_equal(result[0]['fee'], result[1]['fee'], result[2]['fee'])
        assert_equal(result[3]['fee'], result[4]['fee'])
        assert_equal(change[0], change[1])
        assert_equal(output[0], output[1])
        assert_equal(output[0], output[2] + result[2]['fee'])
        assert_equal(change[0] + result[0]['fee'], change[2])
        assert_equal(output[3], output[4] + result[4]['fee'])
        assert_equal(change[3] + result[3]['fee'], change[4])

        inputs = []
        outputs = {self.nodes[2].getnewaddress(): value for value in (1.0, 1.1, 1.2, 1.3)}
        rawtx = self.nodes[3].createrawtransaction(inputs, outputs)

        result = [self.nodes[3].fundrawtransaction(rawtx),
                  # split the fee between outputs 0, 2, and 3, but not output 1
                  self.nodes[3].fundrawtransaction(rawtx, {"subtractFeeFromOutputs": [0, 2, 3]})]

        dec_tx = [self.nodes[3].decoderawtransaction(result[0]['hex']),
                  self.nodes[3].decoderawtransaction(result[1]['hex'])]

        # Nested list of non-change output amounts for each transaction.
        output = [[out['value'] for i, out in enumerate(d['vout']) if i != r['changepos']]
                  for d, r in zip(dec_tx, result)]

        # List of differences in output amounts between normal and subtractFee transactions.
        share = [o0 - o1 for o0, o1 in zip(output[0], output[1])]

        # Output 1 is the same in both transactions.
        assert_equal(share[1], 0)

        # The other 3 outputs are smaller as a result of subtractFeeFromOutputs.
        assert_greater_than(share[0], 0)
        assert_greater_than(share[2], 0)
        assert_greater_than(share[3], 0)

        # Outputs 2 and 3 take the same share of the fee.
        assert_equal(share[2], share[3])

        # Output 0 takes at least as much share of the fee, and no more than 2
        # satoshis more, than outputs 2 and 3.
        assert_greater_than_or_equal(share[0], share[2])
        assert_greater_than_or_equal(share[2] + Decimal(2e-8), share[0])

        # The fee is the same in both transactions.
        assert_equal(result[0]['fee'], result[1]['fee'])

        # The total subtracted from the outputs is equal to the fee.
        assert_equal(share[0] + share[2] + share[3], result[0]['fee'])


if __name__ == '__main__':
    RawTransactionsTest().main()
