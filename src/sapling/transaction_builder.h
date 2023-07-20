// Copyright (c) 2018-2020 The Zcash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef TRANSACTION_BUILDER_H
#define TRANSACTION_BUILDER_H

#include "coins.h"
#include "consensus/params.h"
#include "keystore.h"
#include "optional.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "uint256.h"
#include "sapling/address.h"
#include "sapling/incrementalmerkletree.h"
#include "sapling/note.h"
#include "sapling/noteencryption.h"

struct SpendDescriptionInfo {
    libzcash::SaplingExpandedSpendingKey expsk;
    libzcash::SaplingNote note;
    uint256 alpha;
    uint256 anchor;
    SaplingWitness witness;

    SpendDescriptionInfo(
        const libzcash::SaplingExpandedSpendingKey& _expsk,
        const libzcash::SaplingNote& _note,
        const uint256& _anchor,
        const SaplingWitness& _witness);
};

struct OutputDescriptionInfo {
    uint256 ovk;
    libzcash::SaplingNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;

    OutputDescriptionInfo(
        const uint256& _ovk,
        const libzcash::SaplingNote& _note,
        const std::array<unsigned char, ZC_MEMO_SIZE>& _memo):
            ovk(_ovk),
            note(_note),
            memo(_memo)
    {}

    Optional<OutputDescription> Build(void* ctx);
};

struct TransparentInputInfo {
    CScript scriptPubKey;
    CAmount value;

    TransparentInputInfo(const CScript& _scriptPubKey, CAmount _value):
        scriptPubKey(_scriptPubKey),
        value(_value)
    {}
};

// Dummy constants used during fee-calculation loop
extern const OutputDescription DUMMY_SHIELD_OUT;
extern const SpendDescription DUMMY_SHIELD_SPEND;
extern const SaplingTxData::binding_sig_t DUMMY_SHIELD_BINDSIG;

class TransactionBuilderResult {
private:
    Optional<CTransaction> maybeTx;
    Optional<std::string> maybeError;
public:
    TransactionBuilderResult() = delete;
    TransactionBuilderResult(const CTransaction& tx);
    TransactionBuilderResult(const std::string& error);
    bool IsTx();
    bool IsError();
    CTransaction GetTxOrThrow();
    Optional<CTransaction> GetTx();
    std::string GetError();
};

class TransactionBuilder
{
private:
    Consensus::Params consensusParams;
    const CKeyStore* keystore;
    CMutableTransaction mtx;
    CAmount fee = -1;   // Verified in Build(). Must be set before.

    std::vector<SpendDescriptionInfo> spends;
    std::vector<OutputDescriptionInfo> outputs;
    std::vector<TransparentInputInfo> tIns;

    Optional<std::pair<uint256, libzcash::SaplingPaymentAddress>> saplingChangeAddr;
    Optional<CTxDestination> tChangeAddr;

public:
    TransactionBuilder(
        const Consensus::Params& consensusParams,
        CKeyStore* keyStore = nullptr);

    void Clear();

    void SetFee(CAmount _fee);

    // Throws if the anchor does not match the anchor used by
    // previously-added Sapling spends.
    void AddSaplingSpend(
        const libzcash::SaplingExpandedSpendingKey& expsk,
        const libzcash::SaplingNote& note,
        const uint256& anchor,
        const SaplingWitness& witness);

    void AddSaplingOutput(
        const uint256& ovk,
        const libzcash::SaplingPaymentAddress& to,
        CAmount value,
        const std::array<unsigned char, ZC_MEMO_SIZE>& memo);

    void AddSaplingOutput(
        const uint256& ovk,
        const libzcash::SaplingPaymentAddress& to,
        CAmount value)
    {
        const std::array<unsigned char, ZC_MEMO_SIZE> memo = {{0xF6}};
        AddSaplingOutput(ovk, to, value, memo);
    }

    // Assumes that the value correctly corresponds to the provided UTXO.
    void AddTransparentInput(const COutPoint& utxo, const CScript& scriptPubKey, CAmount value);

    void AddTransparentOutput(const CTxOut& out);
    void AddTransparentOutput(const CTxDestination& dest, CAmount value);

    void SendChangeTo(const libzcash::SaplingPaymentAddress& changeAddr, const uint256& ovk);

    void SendChangeTo(const CTxDestination& changeAddr);

    TransactionBuilderResult Build(bool fDummySig = false);
    // Add Sapling Spend/Output descriptions, binding sig, and transparent signatures
    TransactionBuilderResult ProveAndSign();
    // Add dummy Sapling Spend/Output descriptions, binding sig, and transparent signatures
    TransactionBuilderResult AddDummySignatures();
    // Remove Sapling Spend/Output descriptions, binding sig, and transparent signatures
    void ClearProofsAndSignatures();
};

#endif /* TRANSACTION_BUILDER_H */
