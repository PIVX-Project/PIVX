// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
#include "primitives/transaction.h"
#include "sapling/sapling_validation.h"
#include "tiertwo/specialtx_validation.h"
#include "test/librust/utiltest.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(special_tx_validation_test)
{
    // First check, sapling not active, transaction with extra payload
    CMutableTransaction mtx;
    mtx.extraPayload = std::vector<uint8_t>(10, 1);
    CValidationState state;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, false));

    // Now activate sapling
    RegtestActivateSapling();

    // After Sapling activation.
    // v1 can only be Type=0
    mtx.nType = 1;
    mtx.nVersion = CTransaction::TxVersion::LEGACY;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("not supported with version 0"));

    // version >= Sapling, type = 0, payload != null.
    mtx.nType = 0;
    mtx.nVersion = CTransaction::TxVersion::SAPLING;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("doesn't support extra payload"));

    // version >= Sapling, type = 0, payload == null --> pass
    mtx.extraPayload = nullopt;
    BOOST_CHECK(CheckSpecialTx(CTransaction(mtx), state, true));

    // nVersion>=2 and nType!=0 without extrapayload
    mtx.nType = 1;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("without extra payload"));

    // Size limits
    mtx.extraPayload = std::vector<uint8_t>(10001, 1);
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("Special tx payload oversize"));

    // Remove two elements, so now it passes the size check
    mtx.extraPayload->pop_back();
    mtx.extraPayload->pop_back();
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("with invalid type"));

    RegtestDeactivateSapling();
}

void test_simple_sapling_invalidity(CMutableTransaction& tx)
{
    CAmount nDummyValueOut;
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        BOOST_CHECK(!CheckTransaction(newTx, false, false, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vin-empty");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.sapData->vShieldSpend.emplace_back();
        newTx.sapData->vShieldSpend[0].nullifier = GetRandHash();

        BOOST_CHECK(!CheckTransaction(newTx, false, false, state, false));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-vout-empty");
    }
    {
        // Ensure that nullifiers are never duplicated within a transaction.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.sapData->vShieldSpend.emplace_back();
        newTx.sapData->vShieldSpend[0].nullifier = GetRandHash();

        newTx.sapData->vShieldOutput.emplace_back();

        newTx.sapData->vShieldSpend.emplace_back();
        newTx.sapData->vShieldSpend[1].nullifier = newTx.sapData->vShieldSpend[0].nullifier;

        BOOST_CHECK(!SaplingValidation::CheckTransactionWithoutProofVerification(newTx, state, nDummyValueOut));
        BOOST_CHECK(state.GetRejectReason() == "bad-spend-description-nullifiers-duplicate");

        newTx.sapData->vShieldSpend[1].nullifier = GetRandHash();

        BOOST_CHECK(SaplingValidation::CheckTransactionWithoutProofVerification(newTx, state, nDummyValueOut));
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        // Create a coinbase transaction
        CTxIn vin;
        vin.prevout = COutPoint();
        newTx.vin.emplace_back(vin);
        CTxOut vout;
        vout.nValue = 2;
        newTx.vout.emplace_back(vout);

        newTx.sapData->vShieldSpend.emplace_back();

        BOOST_CHECK(!CheckTransaction(newTx, false, false, state, false, false, true));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-sapling");
    }
    {
        CMutableTransaction newTx(tx);
        CValidationState state;

        // Create a coinstake transaction
        CTxIn vin;
        vin.prevout = COutPoint(UINT256_ZERO, 0);
        newTx.vin.emplace_back(vin);
        CTxOut vout;
        vout.nValue = 0;
        newTx.vout.emplace_back(vout);
        vout.nValue = 2;
        newTx.vout.emplace_back(vout);

        newTx.sapData->vShieldSpend.emplace_back();

        BOOST_CHECK(!CheckTransaction(newTx, false, false, state, false, false, true));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-invalid-sapling");
    }
}

BOOST_AUTO_TEST_CASE(test_simple_shield_invalid)
{
    // Switch to regtest parameters so we can activate Sapling
    SelectParams(CBaseChainParams::REGTEST);

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::TxVersion::SAPLING;

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    test_simple_sapling_invalidity(mtx);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

    // Switch back to mainnet parameters as originally selected in test fixture
    SelectParams(CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_SUITE_END()
