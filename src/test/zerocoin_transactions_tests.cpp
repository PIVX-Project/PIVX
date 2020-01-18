// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/CoinRandomnessSchnorrSignature.h"
#include "amount.h"
#include "chainparams.h"
#include "coincontrol.h"
#include "consensus/zerocoin_verify.h"
#include "main.h"
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
    std::list<std::pair<CBitcoinAddress*, CAmount>> outputs;
    cWallet.SpendZerocoin(nAmount, *wtx, receipt, vMints, fMintChange, fMinimizeChange, outputs);

    BOOST_CHECK_MESSAGE(receipt.GetStatus() == ZPIV_TRX_FUNDS_PROBLEMS, strprintf("Failed Invalid Amount Check: %s", receipt.GetStatusMessage()));

    nAmount = 1;
    CZerocoinSpendReceipt receipt2;
    cWallet.SpendZerocoin(nAmount, *wtx, receipt2, vMints, fMintChange, fMinimizeChange, outputs);

    // if using "wallet.dat", instead of "unlocked.dat" need this
    /// BOOST_CHECK_MESSAGE(vString == "Error: Wallet locked, unable to create transaction!"," Locked Wallet Check Failed");

    BOOST_CHECK_MESSAGE(receipt2.GetStatus() == ZPIV_TRX_FUNDS_PROBLEMS, strprintf("Failed Invalid Amount Check: %s", receipt.GetStatusMessage()));

}

BOOST_AUTO_TEST_CASE(zerocoin_schnorr_signature_test)
{
    const int NUM_OF_TESTS = 50;
    SelectParams(CBaseChainParams::MAIN);
    libzerocoin::ZerocoinParams *ZCParams_v1 = Params().Zerocoin_Params(true);
    (void)ZCParams_v1;
    libzerocoin::ZerocoinParams *ZCParams_v2 = Params().Zerocoin_Params(false);
    (void)ZCParams_v2;

    for (int i=0; i<NUM_OF_TESTS; i++) {

        // mint a v1 coin
        CBigNum s = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
        CBigNum r = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
        CBigNum c = ZCParams_v1->coinCommitmentGroup.g.pow_mod(s, ZCParams_v1->coinCommitmentGroup.modulus).mul_mod(
                ZCParams_v1->coinCommitmentGroup.h.pow_mod(r, ZCParams_v1->coinCommitmentGroup.modulus), ZCParams_v1->coinCommitmentGroup.modulus);
        for (uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {
            if (c.isPrime(ZEROCOIN_MINT_PRIME_PARAM)) break;
            CBigNum r_delta = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
            r = (r + r_delta) % ZCParams_v1->coinCommitmentGroup.groupOrder;
            c = ZCParams_v1->coinCommitmentGroup.g.pow_mod(s, ZCParams_v1->coinCommitmentGroup.modulus).mul_mod(
                    ZCParams_v1->coinCommitmentGroup.h.pow_mod(r, ZCParams_v1->coinCommitmentGroup.modulus), ZCParams_v1->coinCommitmentGroup.modulus);
        }
        BOOST_CHECK_MESSAGE(c.isPrime(ZEROCOIN_MINT_PRIME_PARAM), "Unable to mint v1 coin");
        libzerocoin::PrivateCoin privCoin_v1(ZCParams_v1, libzerocoin::CoinDenomination::ZQ_ONE, s, r);
        const CBigNum randomness_v1 = privCoin_v1.getRandomness();
        const CBigNum pubCoinValue_v1 = privCoin_v1.getPublicCoin().getValue();
        const CBigNum serialNumber_v1 = privCoin_v1.getSerialNumber();

        // mint a v2 coin
        libzerocoin::PrivateCoin privCoin_v2(ZCParams_v2, libzerocoin::CoinDenomination::ZQ_ONE, true);
        const CBigNum randomness_v2 = privCoin_v2.getRandomness();
        const CBigNum pubCoinValue_v2 = privCoin_v2.getPublicCoin().getValue();
        const CBigNum serialNumber_v2 = privCoin_v2.getSerialNumber();

        // get a random msghash
        const uint256 msghash = CBigNum::randKBitBignum(256).getuint256();

        // sign the msghash with the randomness of the v1 coin
        libzerocoin::CoinRandomnessSchnorrSignature crss_v1(ZCParams_v1, randomness_v1, msghash);
        CDataStream ser_crss_v1(SER_NETWORK, PROTOCOL_VERSION);
        ser_crss_v1 << crss_v1;

        // sign the msghash with the randomness of the v2 coin
        libzerocoin::CoinRandomnessSchnorrSignature crss_v2(ZCParams_v2, randomness_v2, msghash);
        CDataStream ser_crss_v2(SER_NETWORK, PROTOCOL_VERSION);
        ser_crss_v2 << crss_v2;

        // unserialize the v1 signature into a fresh object and verify it
        libzerocoin::CoinRandomnessSchnorrSignature new_crss_v1(ser_crss_v1);
        BOOST_CHECK_MESSAGE(
                new_crss_v1.Verify(ZCParams_v1, serialNumber_v1, pubCoinValue_v1, msghash),
                "Failed to verify schnorr signature with v1 coin"
                );

        // unserialize the v2 signature into a fresh object and verify it
        libzerocoin::CoinRandomnessSchnorrSignature new_crss_v2(ser_crss_v2);
        BOOST_CHECK_MESSAGE(
                new_crss_v2.Verify(ZCParams_v2, serialNumber_v2, pubCoinValue_v2, msghash),
                "Failed to verify schnorr signature with v2 coin"
                );

        // verify failure on different msghash
        uint256 msghash2;
        do {
            msghash2 = CBigNum::randKBitBignum(256).getuint256();
        } while (msghash2 == msghash);
        BOOST_CHECK_MESSAGE(
            !new_crss_v1.Verify(ZCParams_v1, serialNumber_v1, pubCoinValue_v1, msghash2),
            "schnorr signature with v1 coin verifies on wrong msghash"
            );
        BOOST_CHECK_MESSAGE(
            !new_crss_v2.Verify(ZCParams_v2, serialNumber_v2, pubCoinValue_v2, msghash2),
            "schnorr signature with v2 coin verifies on wrong msghash"
            );

        // verify failure swapping serials
        BOOST_CHECK_MESSAGE(
            !new_crss_v1.Verify(ZCParams_v1, serialNumber_v2, pubCoinValue_v1, msghash),
            "schnorr signature with v1 coin verifies on wrong serial"
            );
        BOOST_CHECK_MESSAGE(
            !new_crss_v2.Verify(ZCParams_v2, serialNumber_v1, pubCoinValue_v2, msghash),
            "schnorr signature with v2 coin verifies on wrong serial"
            );

        // verify failure swapping public coins
        BOOST_CHECK_MESSAGE(
            !new_crss_v1.Verify(ZCParams_v1, serialNumber_v1, pubCoinValue_v2, msghash),
            "schnorr signature with v1 coin verifies on wrong public coin value"
            );
        BOOST_CHECK_MESSAGE(
            !new_crss_v2.Verify(ZCParams_v2, serialNumber_v2, pubCoinValue_v1, msghash),
            "schnorr signature with v2 coin verifies on wrong public coin value"
            );

    }

}

BOOST_AUTO_TEST_CASE(zerocoin_public_spend_test)
{
    SelectParams(CBaseChainParams::MAIN);
    libzerocoin::ZerocoinParams *ZCParams_v1 = Params().Zerocoin_Params(true);
    libzerocoin::ZerocoinParams *ZCParams_v2 = Params().Zerocoin_Params(false);
    (void)ZCParams_v1;
    (void)ZCParams_v2;

    // create v1 coin
    CBigNum s = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
    CBigNum r = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
    CBigNum c = ZCParams_v1->coinCommitmentGroup.g.pow_mod(s, ZCParams_v1->coinCommitmentGroup.modulus).mul_mod(
            ZCParams_v1->coinCommitmentGroup.h.pow_mod(r, ZCParams_v1->coinCommitmentGroup.modulus), ZCParams_v1->coinCommitmentGroup.modulus);
    for (uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {
        if (c.isPrime(ZEROCOIN_MINT_PRIME_PARAM)) break;
        CBigNum r_delta = CBigNum::randBignum(ZCParams_v1->coinCommitmentGroup.groupOrder);
        r = (r + r_delta) % ZCParams_v1->coinCommitmentGroup.groupOrder;
        c = ZCParams_v1->coinCommitmentGroup.g.pow_mod(s, ZCParams_v1->coinCommitmentGroup.modulus).mul_mod(
                ZCParams_v1->coinCommitmentGroup.h.pow_mod(r, ZCParams_v1->coinCommitmentGroup.modulus), ZCParams_v1->coinCommitmentGroup.modulus);
    }
    BOOST_CHECK_MESSAGE(c.isPrime(ZEROCOIN_MINT_PRIME_PARAM), "Unable to mint v1 coin");
    libzerocoin::PrivateCoin privCoin_v1(ZCParams_v1, libzerocoin::CoinDenomination::ZQ_ONE, s, r);

    CZerocoinMint mint_v1 = CZerocoinMint(
            privCoin_v1.getPublicCoin().getDenomination(),
            privCoin_v1.getPublicCoin().getValue(),
            privCoin_v1.getRandomness(),
            privCoin_v1.getSerialNumber(),
            false,
            1,
            nullptr);

    // create v2 coin
    libzerocoin::PrivateCoin privCoin_v2(ZCParams_v2, libzerocoin::CoinDenomination::ZQ_ONE, true);
    CPrivKey privKey = privCoin_v2.getPrivKey();

    CZerocoinMint mint_v2 = CZerocoinMint(
            privCoin_v2.getPublicCoin().getDenomination(),
            privCoin_v2.getPublicCoin().getValue(),
            privCoin_v2.getRandomness(),
            privCoin_v2.getSerialNumber(),
            false,
            privCoin_v2.getVersion(),
            &privKey);

    // Mint txs
    CTransaction prevTx_v1;
    CTransaction prevTx_v2;

    CScript scriptSerializedCoin_v1 = CScript()
    << OP_ZEROCOINMINT << privCoin_v1.getPublicCoin().getValue().getvch().size() << privCoin_v1.getPublicCoin().getValue().getvch();
    CTxOut out_v1 = CTxOut(libzerocoin::ZerocoinDenominationToAmount(privCoin_v1.getPublicCoin().getDenomination()), scriptSerializedCoin_v1);
    prevTx_v1.vout.push_back(out_v1);

    CScript scriptSerializedCoin_v2 = CScript()
    << OP_ZEROCOINMINT << privCoin_v2.getPublicCoin().getValue().getvch().size() << privCoin_v2.getPublicCoin().getValue().getvch();
    CTxOut out_v2 = CTxOut(libzerocoin::ZerocoinDenominationToAmount(privCoin_v2.getPublicCoin().getDenomination()), scriptSerializedCoin_v2);
    prevTx_v2.vout.push_back(out_v2);

    mint_v1.SetOutputIndex(0);
    mint_v1.SetTxHash(prevTx_v1.GetHash());

    mint_v2.SetOutputIndex(0);
    mint_v2.SetTxHash(prevTx_v2.GetHash());

    // Spend txs
    CMutableTransaction tx1, tx2, tx3;
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 1*CENT;
    tx1.vout[0].scriptPubKey = GetScriptForDestination(CBitcoinAddress("D9Ti4LEhF1n6dR2hGd2SyNADD51AVgva6q").Get());
    tx2.vout.resize(1);
    tx2.vout[0].nValue = 1*CENT;
    tx2.vout[0].scriptPubKey = GetScriptForDestination(CBitcoinAddress("D9Ti4LEhF1n6dR2hGd2SyNADD51AVgva6q").Get());
    tx3.vout.resize(1);
    tx3.vout[0].nValue = 1*CENT;
    tx3.vout[0].scriptPubKey = GetScriptForDestination(CBitcoinAddress("D9Ti4LEhF1n6dR2hGd2SyNADD51AVgva6q").Get());

    CTxIn in1, in2, in3;

    // check spendVersion = 3 for v2 coins
    // -----------------------------------
    int spendVersion = 3;
    BOOST_CHECK_MESSAGE(ZPIVModule::createInput(in1, mint_v2, tx1.GetHash(), spendVersion),
            "Failed to create zc input for mint v2 and spendVersion 3");

    std::cout << "Spend v3 size: " << ::GetSerializeSize(in1, SER_NETWORK, PROTOCOL_VERSION) << " bytes" << std::endl;

    PublicCoinSpend publicSpend1(ZCParams_v2);
    BOOST_CHECK_MESSAGE(ZPIVModule::validateInput(in1, out_v2, tx1, publicSpend1),
            "Failed to validate zc input for mint v2 and spendVersion 3");

    // Verify that it fails with a different denomination
    in1.nSequence = 500;
    PublicCoinSpend publicSpend1b(ZCParams_v2);
    BOOST_CHECK_MESSAGE(!ZPIVModule::validateInput(in1, out_v2, tx1, publicSpend1b), "Different denomination for mint v2 and spendVersion 3");

    // check spendVersion = 4 for v2 coins
    // -----------------------------------
    spendVersion = 4;
    BOOST_CHECK_MESSAGE(ZPIVModule::createInput(in2, mint_v2, tx2.GetHash(), spendVersion),
            "Failed to create zc input for mint v2 and spendVersion 4");

    std::cout << "Spend v4 (coin v2) size: " << ::GetSerializeSize(in2, SER_NETWORK, PROTOCOL_VERSION) << " bytes" << std::endl;

    PublicCoinSpend publicSpend2(ZCParams_v2);
    BOOST_CHECK_MESSAGE(ZPIVModule::validateInput(in2, out_v2, tx2, publicSpend2),
            "Failed to validate zc input for mint v2 and spendVersion 4");

    // Verify that it fails with a different denomination
    in2.nSequence = 500;
    PublicCoinSpend publicSpend2b(ZCParams_v2);
    BOOST_CHECK_MESSAGE(!ZPIVModule::validateInput(in2, out_v2, tx2, publicSpend2b), "Different denomination for mint v2 and spendVersion 4");

    // check spendVersion = 4 for v1 coins
    // -----------------------------------
    BOOST_CHECK_MESSAGE(ZPIVModule::createInput(in3, mint_v1, tx3.GetHash(), spendVersion),
            "Failed to create zc input for mint v1 and spendVersion 4");

    std::cout << "Spend v4 (coin v1) size: " << ::GetSerializeSize(in3, SER_NETWORK, PROTOCOL_VERSION) << " bytes" << std::endl;

    PublicCoinSpend publicSpend3(ZCParams_v1);
    BOOST_CHECK_MESSAGE(ZPIVModule::validateInput(in3, out_v1, tx3, publicSpend3),
            "Failed to validate zc input for mint v1 and spendVersion 4");

    // Verify that it fails with a different denomination
    in3.nSequence = 500;
    PublicCoinSpend publicSpend3b(ZCParams_v1);
    BOOST_CHECK_MESSAGE(!ZPIVModule::validateInput(in3, out_v1, tx3, publicSpend3b), "Different denomination for mint v1 and spendVersion 4");

}

BOOST_AUTO_TEST_SUITE_END()
