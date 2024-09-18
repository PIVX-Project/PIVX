// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_CONSENSUS_TX_VERIFY_H
#define PIVX_CONSENSUS_TX_VERIFY_H

#include "chainparams.h"
#include "primitives/transaction.h"

#include <stdint.h>
#include <vector>

class CBlockIndex;
class CChainParams;
class CCoinsViewCache;
class CValidationState;

/** Transaction validation functions */

/** Context-independent validity checks */
bool CheckTransaction(const CTransaction& tx, CValidationState& state, bool fColdStakingActive);
/** Context-dependent validity checks */
bool ContextualCheckTransaction(const CTransactionRef& tx, CValidationState& state, const CChainParams& chainparams, int nHeight, bool isMined, bool fIBD);

/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 *
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransactionRef& tx, int nBlockHeight, int64_t nBlockTime = 0);

#endif // PIVX_CONSENSUS_TX_VERIFY_H
