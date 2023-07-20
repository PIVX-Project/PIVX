// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "test/fuzz/fuzz.h"
#include "version.h"

#include <cassert>

bool CheckTransactionAllFlagsDisabled(const CTransaction& tx, CValidationState state);
bool CheckTransactionColdStakingEnabled(const CTransaction& tx, CValidationState state);

void test_one_input(std::vector<uint8_t> buffer)
{
    CDataStream ds(buffer, SER_NETWORK, INIT_PROTO_VERSION);
    try {
        int nVersion;
        ds >> nVersion;
        ds.SetVersion(nVersion);
    } catch (const std::ios_base::failure&) {
        return;
    }
    unsigned int nBlockHeight = 0;
    try {
        ds >> nBlockHeight;
    } catch (const std::ios_base::failure& e) {
        return;
    }
    bool valid_tx = true;
    const CTransaction tx = [&] {
        try {
            return CTransaction(deserialize, ds);
        } catch (const std::ios_base::failure&) {
            valid_tx = false;
            return CTransaction();
        }
    }();
    bool valid_mutable_tx = true;
    CDataStream ds_mtx(buffer, SER_NETWORK, INIT_PROTO_VERSION);
    CMutableTransaction mutable_tx;
    try {
        int nVersion;
        ds_mtx >> nVersion;
        ds_mtx.SetVersion(nVersion);
        unsigned int nBlockHeight2 = 0;
        ds_mtx >> nBlockHeight2;
        ds_mtx >> mutable_tx;
    } catch (const std::ios_base::failure&) {
        valid_mutable_tx = false;
    }
    assert(valid_tx == valid_mutable_tx);
    if (!valid_tx) {
        return;
    }

    // Cold staking flag disabled
    CValidationState state_with_all_flags_disabled;
    CheckTransactionAllFlagsDisabled(tx, state_with_all_flags_disabled);

    // Cold Staking enabled
    CValidationState state_with_cold_staking_enabled;
    CheckTransactionColdStakingEnabled(tx, state_with_cold_staking_enabled);

    CTransactionRef ptx = MakeTransactionRef(tx);
    std::string reason;
    IsStandardTx(ptx, nBlockHeight,reason);

    CValidationState contextualCheckState;
    ContextualCheckTransaction(ptx, contextualCheckState, Params(), nBlockHeight, true, false);

    (void)tx.GetHash();
    (void)tx.GetTotalSize();
    try {
        (void)tx.GetValueOut();
    } catch (const std::runtime_error&) {
    }
    (void)tx.IsCoinBase();
    (void)tx.IsCoinStake();
    (void)tx.IsShieldedTx();
    (void)tx.IsSpecialTx();
    (void)tx.IsNormalType();
    (void)tx.isSaplingVersion();
    (void)tx.hasSaplingData();
    (void)tx.HasP2CSOutputs();
    (void)tx.ContainsZerocoins();
    (void)tx.IsNull();
    (void)tx.ToString();


    (void)EncodeHexTx(tx);
    (void)GetLegacySigOpCount(tx);
    (void)IsFinalTx(ptx, nBlockHeight, /* nBlockTime= */ 1024);
    (void)IsStandardTx(ptx, nBlockHeight, reason);
}

bool CheckTransactionAllFlagsDisabled(const CTransaction& tx, CValidationState state) {
    return CheckTransaction(tx, state, false);
}

bool CheckTransactionColdStakingEnabled(const CTransaction& tx, CValidationState state) {
    return CheckTransaction(tx, state, true);
}

