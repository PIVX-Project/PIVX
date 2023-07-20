// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/librust/sapling_test_fixture.h"
#include "test/librust/utiltest.h"

#include "sapling/sapling.h"
#include "sapling/transaction_builder.h"
#include "sapling/sapling_validation.h"

#include <univalue.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sapling_transaction_builder_tests, SaplingRegTestingSetup)

BOOST_AUTO_TEST_CASE(TransparentToSapling)
{
    auto consensusParams = Params().GetConsensus();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto sk_from = libzcash::SaplingSpendingKey::random();
    auto fvk_from = sk_from.full_viewing_key();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto fvk = sk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    diversifier_t d = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto pk = *ivk.address(d);

    // Create a shielding transaction from transparent to Sapling
    // 0.5 t-PIV in, 0.4 shielded-PIV out, 0.1 t-PIV fee
    auto builder = TransactionBuilder(consensusParams, &keystore);
    builder.AddTransparentInput(COutPoint(uint256S("1234"), 0), scriptPubKey, 50000000);
    builder.AddSaplingOutput(fvk_from.ovk, pk, 40000000, {});
    builder.SetFee(10000000);
    auto tx = builder.Build().GetTxOrThrow();

    BOOST_CHECK_EQUAL(tx.vin.size(), 1);
    BOOST_CHECK_EQUAL(tx.vout.size(), 0);
    BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 0);
    BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 1);
    BOOST_CHECK_EQUAL(tx.sapData->valueBalance, -40000000);

    CValidationState state;
    BOOST_CHECK(SaplingValidation::ContextualCheckTransaction(tx, state, Params(), 2, true, false));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "");
}

BOOST_AUTO_TEST_CASE(SaplingToSapling)
{
    auto consensusParams = Params().GetConsensus();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    // Create a Sapling-only transaction
    // --- 0.4 shielded-PIV in, 0.299 shielded-PIV out, 0.1 shielded-PIV fee, 0.001 shielded-PIV change (added to fee)
    auto testNote = GetTestSaplingNote(pa, 40000000);
    auto builder = TransactionBuilder(consensusParams);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.SetFee(10000000);

    // Check that trying to add a different anchor fails
    // TODO: the following check can be split out in to another test
    BOOST_CHECK_THROW(builder.AddSaplingSpend(expsk, testNote.note, uint256(), testNote.tree.witness()), std::runtime_error);

    builder.AddSaplingOutput(fvk.ovk, pa, 29900000, {});
    auto tx = builder.Build().GetTxOrThrow();

    BOOST_CHECK_EQUAL(tx.vin.size(), 0);
    BOOST_CHECK_EQUAL(tx.vout.size(), 0);
    BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 1);
    // since the change is below the dust threshold, it is added to the fee
    BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 1);
    BOOST_CHECK_EQUAL(tx.sapData->valueBalance, 10100000);

    CValidationState state;
    BOOST_CHECK(SaplingValidation::ContextualCheckTransaction(tx, state, Params(), 3, true, false));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "");

    // --- Now try with 1 shielded-PIV in, 0.5 shielded-PIV out, 0.1 shielded-PIV fee, 0.4 shielded-PIV change
    auto testNote2 = GetTestSaplingNote(pa, 100000000);
    auto builder2 = TransactionBuilder(consensusParams);
    builder2.AddSaplingSpend(expsk, testNote2.note, testNote2.tree.root(), testNote2.tree.witness());
    builder2.SetFee(10000000);
    builder2.AddSaplingOutput(fvk.ovk, pa, 50000000, {});
    auto tx2 = builder2.Build().GetTxOrThrow();
    BOOST_CHECK_EQUAL(tx2.vin.size(), 0);
    BOOST_CHECK_EQUAL(tx2.vout.size(), 0);
    BOOST_CHECK_EQUAL(tx2.sapData->vShieldedSpend.size(), 1);
    BOOST_CHECK_EQUAL(tx2.sapData->vShieldedOutput.size(), 2);
    BOOST_CHECK_EQUAL(tx2.sapData->valueBalance, 10000000);
    BOOST_CHECK(SaplingValidation::ContextualCheckTransaction(tx2, state, Params(), 3, true, false));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "");
}

BOOST_AUTO_TEST_CASE(ThrowsOnTransparentInputWithoutKeyStore)
{
    auto builder = TransactionBuilder(Params().GetConsensus());
    BOOST_CHECK_THROW(builder.AddTransparentInput(COutPoint(), CScript(), 1), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(RejectsInvalidTransparentOutput)
{
    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(Params().GetConsensus());
    BOOST_CHECK_THROW(builder.AddTransparentOutput(taddr, 50), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(RejectsInvalidTransparentChangeAddress)
{
    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(Params().GetConsensus());
    BOOST_CHECK_THROW(builder.SendChangeTo(taddr), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(FailsWithNegativeChange)
{
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    // Generate a 0.5 PIV note
    auto testNote = GetTestSaplingNote(pa, 59990000);

    // Fail if there is only a Sapling output
    // 0.5 shielded-PIV out, 0.1 t-PIV fee
    auto builder = TransactionBuilder(consensusParams);
    builder.AddSaplingOutput(fvk.ovk, pa, 50000000, {});
    builder.SetFee(10000000);
    BOOST_CHECK_EQUAL("Change cannot be negative", builder.Build().GetError());

    // Fail if there is only a transparent output
    // 0.5 t-PIV out, 0.1 t-PIV fee
    builder = TransactionBuilder(consensusParams, &keystore);
    builder.AddTransparentOutput(taddr, 50000000);
    builder.SetFee(10000000);
    BOOST_CHECK_EQUAL("Change cannot be negative", builder.Build().GetError());

    // Fails if there is insufficient input
    // 0.5 t-PIV out, 0.1 t-PIV fee, 0.59999 shielded-PIV in
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    BOOST_CHECK_EQUAL("Change cannot be negative", builder.Build().GetError());

    // Succeeds if there is sufficient input
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 10000);
    BOOST_CHECK(builder.Build().IsTx());
}

BOOST_AUTO_TEST_CASE(ChangeOutput)
{
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 25000000);

    // Generate change Sapling address
    auto sk2 = libzcash::SaplingSpendingKey::random();
    auto fvkOut = sk2.full_viewing_key();
    auto zChangeAddr = sk2.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    // No change address and no Sapling spends
    {
        auto builder = TransactionBuilder(consensusParams, &keystore);
        builder.SetFee(10000000);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000000);
        BOOST_CHECK_EQUAL("Could not determine change address", builder.Build().GetError());
    }

    // Change to the same address as the first Sapling spend
    {
        auto builder = TransactionBuilder(consensusParams, &keystore);
        builder.SetFee(10000000);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000000);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        auto tx = builder.Build().GetTxOrThrow();

        BOOST_CHECK_EQUAL(tx.vin.size(), 1);
        BOOST_CHECK_EQUAL(tx.vout.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 1);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 1);
        BOOST_CHECK_EQUAL(tx.sapData->valueBalance, -15000000);
    }

    // Change to a Sapling address
    {
        auto builder = TransactionBuilder(consensusParams, &keystore);
        builder.SetFee(10000000);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000000);
        builder.SendChangeTo(zChangeAddr, fvkOut.ovk);
        auto tx = builder.Build().GetTxOrThrow();

        BOOST_CHECK_EQUAL(tx.vin.size(), 1);
        BOOST_CHECK_EQUAL(tx.vout.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 1);
        BOOST_CHECK_EQUAL(tx.sapData->valueBalance, -15000000);
    }

    // Change to a transparent address
    {
        auto builder = TransactionBuilder(consensusParams, &keystore);
        builder.SetFee(10000000);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000000);
        builder.SendChangeTo(taddr);
        auto tx = builder.Build().GetTxOrThrow();

        BOOST_CHECK_EQUAL(tx.vin.size(), 1);
        BOOST_CHECK_EQUAL(tx.vout.size(), 1);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->valueBalance, 0);
        BOOST_CHECK_EQUAL(tx.vout[0].nValue, 15000000);
    }
}

BOOST_AUTO_TEST_CASE(SetFee)
{
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 5 * COIN);

    // Configured fee (auto with SaplingOperation)
    {
        auto builder = TransactionBuilder(consensusParams);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 3 * COIN, {});

        builder.SetFee(COIN);
        auto tx = builder.Build().GetTxOrThrow();

        BOOST_CHECK_EQUAL(tx.vin.size(), 0);
        BOOST_CHECK_EQUAL(tx.vout.size(), 0);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedSpend.size(), 1);
        BOOST_CHECK_EQUAL(tx.sapData->vShieldedOutput.size(), 2);
        BOOST_CHECK_EQUAL(tx.sapData->valueBalance, COIN);
    }
}

BOOST_AUTO_TEST_CASE(CheckSaplingTxVersion)
{
    auto consensusParams = Params().GetConsensus();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pk = sk.default_address();

    // Cannot add Sapling outputs to a non-Sapling transaction
    auto builder = TransactionBuilder(consensusParams);
    builder.SetFee(10000000);
    try {
        builder.AddSaplingOutput(uint256(), pk, 12345, {});
    } catch (std::runtime_error const & err) {
        BOOST_CHECK_EQUAL(err.what(), std::string("TransactionBuilder cannot add Sapling output to pre-Sapling transaction"));
    } catch(...) {
        BOOST_CHECK_MESSAGE(false, "Expected std::runtime_error");
    }

    // Cannot add Sapling spends to a non-Sapling transaction
    libzcash::SaplingNote note(pk, 50000);
    SaplingMerkleTree tree;
    try {
        builder.AddSaplingSpend(expsk, note, uint256(), tree.witness());
    } catch (std::runtime_error const & err) {
        BOOST_CHECK_EQUAL(err.what(), std::string("TransactionBuilder cannot add Sapling spend to pre-Sapling transaction"));
    } catch(...) {
        BOOST_CHECK_MESSAGE(false, "Expected std::runtime_error");
    }
}


BOOST_AUTO_TEST_SUITE_END()
