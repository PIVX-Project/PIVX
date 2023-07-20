// Copyright (c) 2016-2020 The Zcash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef hemis_UTIL_TEST_H
#define hemis_UTIL_TEST_H

#include "sapling/address.h"
#include "sapling/incrementalmerkletree.h"
#include "sapling/note.h"
#include "sapling/noteencryption.h"
#include "wallet/wallet.h"

struct TestSaplingNote {
    libzcash::SaplingNote note;
    SaplingMerkleTree tree;
};

struct ShieldedDestination {
    const libzcash::SaplingExtendedSpendingKey &sk;
    CAmount amount;
};

struct TransparentInput {
    COutPoint outPoint;
    CScript scriptPubKey;
    CAmount amount;
};


libzcash::SaplingExtendedSpendingKey GetTestMasterSaplingSpendingKey();

CKey AddTestCKeyToKeyStore(CBasicKeyStore& keyStore, bool genNewKey = false);
CKey AddTestCKeyToWallet(CWallet& wallet, bool genNewKey = false);

/**
 * Generates a dummy destination script
 */
CScript CreateDummyDestinationScript();

/**
 * Generate a dummy SaplingNote and a SaplingMerkleTree with that note's commitment.
 */
TestSaplingNote GetTestSaplingNote(const libzcash::SaplingPaymentAddress& pa, CAmount value);


/**
 * One or many inputs from keyStoreFrom, one or many shielded outputs to pwalletIn (if not nullptr).
 */
CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStoreFrom,
                                 std::vector<TransparentInput> vIn,
                                 std::vector<ShieldedDestination> vDest,
                                 const CWallet* pwalletIn = nullptr);

/**
 * Single dummy input, one or many shielded outputs.
 */
CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CWallet& keyStoreFrom,
                                 CAmount inputAmount,
                                 std::vector<ShieldedDestination> vDest,
                                 bool genNewKey = false,
                                 const CWallet* pwalletIn = nullptr);

/**
 * Single dummy input, single shielded output to sk default address.
 */
CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CWallet& keyStore,
                                 const libzcash::SaplingExtendedSpendingKey &sk,
                                 CAmount value,
                                 bool genNewKey = false,
                                 const CWallet* pwalletIn = nullptr);

<<<<<<< HEAD
#endif // PIVX_UTIL_TEST_H
=======
#endif // hemis_UTIL_TEST_H
>>>>>>> 1f345019d (first step)
