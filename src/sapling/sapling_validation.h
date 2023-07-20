// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_SAPLING_VALIDATION_H
#define hemis_SAPLING_VALIDATION_H

#include "chainparams.h"

class CTransaction;
class CValidationState;

namespace SaplingValidation {

/** Context-independent validity checks */
// Note: for v3+, if the tx has no shielded data, this method returns true.
// Note2: This function only performs shielded data related checks, it does NOT checks regular inputs and outputs.
bool CheckTransaction(const CTransaction& tx, CValidationState& state, CAmount& nValueOut);
bool CheckTransactionWithoutProofVerification(const CTransaction& tx, CValidationState &state, CAmount& nValueOut);

/** Check a transaction contextually against a set of consensus rules */
// Note: if v5 upgrade wasn't enforced, this method returns true without performing any check.
bool ContextualCheckTransaction(const CTransaction &tx, CValidationState &state,
                                const CChainParams &chainparams, int nHeight, bool isMined,
                                bool sInitBlockDownload);

}; // End SaplingValidation namespace

#endif //hemis_SAPLING_VALIDATION_H
