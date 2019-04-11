// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Denominations.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Accumulator.h"
#include "zpiv/zerocoin.h"
#include "zpiv/deterministicmint.h"
#include "zpiv/zpivwallet.h"
#include "libzerocoin/Coin.h"
#include "amount.h"
#include "chainparams.h"
#include "coincontrol.h"
#include "main.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "txdb.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace libzerocoin;

class CDeterministicMint;

BOOST_AUTO_TEST_SUITE(zerocoin_coinspend_tests)

/**
 * Check that wrapped serials pass and not pass using the new validation.
 */
BOOST_AUTO_TEST_CASE(zerocoin_wrapped_serial_spend_test)
{
    unsigned int TESTS_COINS_TO_ACCUMULATE = 5;

    SelectParams(CBaseChainParams::MAIN);
    ZerocoinParams *ZCParams = Params().Zerocoin_Params(false);
    (void)ZCParams;

    // Seed + Mints
    string strWalletFile = "unittestwallet.dat";
    CWalletDB walletdb(strWalletFile, "cr+");
    CWallet wallet(strWalletFile);
    CzPIVWallet *czPIVWallet = new CzPIVWallet(wallet.strWalletFile);

    // Get the 5 created mints.
    CoinDenomination denom = CoinDenomination::ZQ_FIFTY;
    std::vector<PrivateCoin> vCoins;
    for (unsigned int i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        PrivateCoin coin(ZCParams, denom, false);
        CDeterministicMint dMint;
        czPIVWallet->GenerateDeterministicZPIV(denom, coin, dMint, true);
        czPIVWallet->UpdateCount();
        vCoins.emplace_back(coin);
    }

    // Selected coin
    PrivateCoin coinToSpend = vCoins[0];

    // Accumulate coins
    Accumulator acc(&ZCParams->accumulatorParams, denom);
    AccumulatorWitness accWitness(ZCParams, acc, coinToSpend.getPublicCoin());

    for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        acc += vCoins[i].getPublicCoin();
        if(i != 0) {
            accWitness += vCoins[i].getPublicCoin();
        }
    }

    // Wrapped serial
    Bignum wrappedSerial = coinToSpend.getSerialNumber() + ZCParams->coinCommitmentGroup.groupOrder * CBigNum(2).pow(256) * 2;
    coinToSpend.setSerialNumber(wrappedSerial);

    CoinSpend wrappedSerialSpend(
            ZCParams,
            ZCParams,
            coinToSpend,
            acc,
            0,
            accWitness,
            0,
            SpendType::SPEND
    );

    // first check that the Verify pass without do the invalid range check
    BOOST_CHECK_MESSAGE(wrappedSerialSpend.Verify(acc, false), "ERROR, Invalid coinSpend not passed without range verification");
    // Now must fail..
    BOOST_CHECK_MESSAGE(!wrappedSerialSpend.Verify(acc, true), "ERROR, Invalid coinSpend passed with range verification");

}

BOOST_AUTO_TEST_SUITE_END()
