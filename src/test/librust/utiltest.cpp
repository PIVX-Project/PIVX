// Copyright (c) 2016-2020 The Zcash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "utiltest.h"

#include "key_io.h"
#include "consensus/upgrades.h"
#include "sapling/transaction_builder.h"

#include <array>

static const std::string T_SECRET_REGTEST = "cND2ZvtabDbJ1gucx9GWH6XT9kgTAqfb6cotPt5Q5CyxVDhid2EN";

const Consensus::Params& RegtestActivateSapling() {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    g_IsSaplingActive = true;
    return Params().GetConsensus();
}

void RegtestDeactivateSapling() {
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    g_IsSaplingActive = false;
}

libzcash::SaplingExtendedSpendingKey GetTestMasterSaplingSpendingKey() {
    std::vector<unsigned char, secure_allocator<unsigned char>> rawSeed(32);
    HDSeed seed(rawSeed);
    return libzcash::SaplingExtendedSpendingKey::Master(seed);
}

CKey CreateCkey(bool genNewKey) {
    CKey tsk;
    if (genNewKey) tsk.MakeNewKey(true);
    else tsk = KeyIO::DecodeSecret(T_SECRET_REGTEST);
    if (!tsk.IsValid()) throw std::runtime_error("CreateCkey:: Invalid priv key");
    return tsk;
}

CKey AddTestCKeyToKeyStore(CBasicKeyStore& keyStore, bool genNewKey) {
    CKey tsk = CreateCkey(genNewKey);
    keyStore.AddKey(tsk);
    return tsk;
}

TestSaplingNote GetTestSaplingNote(const libzcash::SaplingPaymentAddress& pa, CAmount value) {
    // Generate dummy Sapling note
    libzcash::SaplingNote note(pa, value);
    uint256 cm = note.cmu().get();
    SaplingMerkleTree tree;
    tree.append(cm);
    return { note, tree };
}

CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStoreFrom,
                                 std::vector<TransparentInput> vIn,
                                 std::vector<ShieldDestination> vDest,
                                 const CWallet* pwalletIn)
{
    auto builder = TransactionBuilder(consensusParams, 1, &keyStoreFrom);
    builder.SetFee(0);

    // From transparent inputs
    for (const auto& in : vIn) {
        builder.AddTransparentInput(in.outPoint, in.scriptPubKey, in.amount);
    }

    // To shield addrs
    for (const auto& dest : vDest) {
        auto fvk = dest.sk.expsk.full_viewing_key();
        auto pa = dest.sk.DefaultAddress();
        builder.AddSaplingOutput(fvk.ovk, pa, dest.amount, {});
    }

    CTransaction tx = builder.Build().GetTxOrThrow();
    CWalletTx wtx {pwalletIn, tx};
    return wtx;
}

// Two dummy input (to trick coinbase check), one or many shield outputs
CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStoreFrom,
                                 CAmount inputAmount,
                                 std::vector<ShieldDestination> vDest,
                                 bool genNewKey,
                                 const CWallet* pwalletIn) {
    // From taddr
    CKey tsk = AddTestCKeyToKeyStore(keyStoreFrom, genNewKey);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    // Two equal dummy inputs to by-pass the coinbase check.
    TransparentInput dummyInput{COutPoint(), scriptPubKey, inputAmount / 2};
    std::vector<TransparentInput> vIn = {dummyInput, dummyInput};
    return GetValidSaplingReceive(consensusParams, keyStoreFrom, vIn, vDest, pwalletIn);
}

// Single input, single shield output
CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStore,
                                 const libzcash::SaplingExtendedSpendingKey &sk,
                                 CAmount value,
                                 bool genNewKey,
                                 const CWallet* pwalletIn) {
    std::vector<ShieldDestination> vDest;
    vDest.push_back({sk, value});
    return GetValidSaplingReceive(
            consensusParams,
            keyStore,
            value,
            vDest,
            genNewKey,
            pwalletIn
    );
}

CScript CreateDummyDestinationScript() {
    CKey key = CreateCkey(true);
    return GetScriptForDestination(key.GetPubKey().GetID());
}