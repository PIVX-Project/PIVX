// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "amount.h"
#include "chainparams.h"
#include "coincontrol.h"
#include "validation.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "txdb.h"
#include "zpiv/zpivmodule.h"
#include "test/test_pivx.h"
#include <boost/test/unit_test.hpp>
#include <iostream>



BOOST_FIXTURE_TEST_SUITE(zerocoin_transactions_tests, TestingSetup)

static CWallet cWallet("unlocked.dat");

BOOST_AUTO_TEST_CASE(zerocoin_spend_test)
{
    SelectParams(CBaseChainParams::MAIN);
    libzerocoin::ZerocoinParams *ZCParams = Params().Zerocoin_Params(false);
    (void)ZCParams;

    bool fFirstRun;
    cWallet.LoadWallet(fFirstRun);
    cWallet.zpivTracker = std::unique_ptr<CzPIVTracker>(new CzPIVTracker(cWallet.strWalletFile));
    CMutableTransaction tx;
    CWalletTx* wtx = new CWalletTx(&cWallet, tx);
    bool fMintChange=true;
    bool fMinimizeChange=true;
    std::vector<CZerocoinSpend> vSpends;
    std::vector<CZerocoinMint> vMints;
    CAmount nAmount = COIN;

    CZerocoinSpendReceipt receipt;
    cWallet.SpendZerocoin(nAmount, *wtx, receipt, vMints, fMintChange, fMinimizeChange);

    BOOST_CHECK_MESSAGE(receipt.GetStatus() == ZPIV_TRX_FUNDS_PROBLEMS, strprintf("Failed Invalid Amount Check: %s", receipt.GetStatusMessage()));

    nAmount = 1;
    CZerocoinSpendReceipt receipt2;
    cWallet.SpendZerocoin(nAmount, *wtx, receipt2, vMints, fMintChange, fMinimizeChange);

    // if using "wallet.dat", instead of "unlocked.dat" need this
    /// BOOST_CHECK_MESSAGE(vString == "Error: Wallet locked, unable to create transaction!"," Locked Wallet Check Failed");

    BOOST_CHECK_MESSAGE(receipt2.GetStatus() == ZPIV_TRX_FUNDS_PROBLEMS, strprintf("Failed Invalid Amount Check: %s", receipt.GetStatusMessage()));

}

BOOST_AUTO_TEST_CASE(zerocoin_public_spend_test)
{
    SelectParams(CBaseChainParams::MAIN);
    libzerocoin::ZerocoinParams *ZCParams = Params().Zerocoin_Params(false);
    (void)ZCParams;

    libzerocoin::PrivateCoin privCoin(ZCParams, libzerocoin::CoinDenomination::ZQ_ONE, true);
    const CPrivKey privKey = privCoin.getPrivKey();

    CZerocoinMint mint = CZerocoinMint(
            privCoin.getPublicCoin().getDenomination(),
            privCoin.getPublicCoin().getValue(),
            privCoin.getRandomness(),
            privCoin.getSerialNumber(),
            false,
            privCoin.getVersion(),
            nullptr);
    mint.SetPrivKey(privKey);


    // Mint tx
    CTransaction prevTx;

    CScript scriptSerializedCoin = CScript()
    << OP_ZEROCOINMINT << privCoin.getPublicCoin().getValue().getvch().size() << privCoin.getPublicCoin().getValue().getvch();
    CTxOut out = CTxOut(libzerocoin::ZerocoinDenominationToAmount(privCoin.getPublicCoin().getDenomination()), scriptSerializedCoin);
    prevTx.vout.push_back(out);

    mint.SetOutputIndex(0);
    mint.SetTxHash(prevTx.GetHash());

    // Spend tx
    CMutableTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1*CENT;
    tx.vout[0].scriptPubKey = GetScriptForDestination(CBitcoinAddress("D9Ti4LEhF1n6dR2hGd2SyNADD51AVgva6q").Get());

    CTxIn in;
    if (!ZPIVModule::createInput(in, mint, tx.GetHash())){
        BOOST_CHECK_MESSAGE(false, "Failed to create zc input");
    }

    PublicCoinSpend publicSpend(ZCParams);
    if (!ZPIVModule::validateInput(in, out, tx, publicSpend)){
        BOOST_CHECK_MESSAGE(false, "Failed to validate zc input");
    }

    PublicCoinSpend publicSpendTest(ZCParams);
    BOOST_CHECK_MESSAGE(ZPIVModule::parseCoinSpend(in, tx, out, publicSpendTest), "Failed to parse public spend");
    libzerocoin::CoinSpend *spend = &publicSpendTest;

    BOOST_CHECK_MESSAGE(publicSpendTest.HasValidSignature(), "Failed to validate public spend signature");
    BOOST_CHECK_MESSAGE(spend->HasValidSignature(), "Failed to validate spend signature");

    // Verify that fails with a different denomination
    in.nSequence = 500;
    PublicCoinSpend publicSpend2(ZCParams);
    BOOST_CHECK_MESSAGE(!ZPIVModule::validateInput(in, out, tx, publicSpend2), "Different denomination");

}

BOOST_AUTO_TEST_SUITE_END()
