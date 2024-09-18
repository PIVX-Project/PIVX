// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_POLICY_POLICY_H
#define PIVX_POLICY_POLICY_H

#include "consensus/consensus.h"
#include "feerate.h"
#include "script/interpreter.h"
#include "script/standard.h"

#include <string>

class CChainParams;
class CCoinsViewCache;
class CTxOut;

/** Default for -blockmaxsize, which controls the maximum size of block the mining code will create **/
static const unsigned int DEFAULT_BLOCK_MAX_SIZE = 750000;
/** Maximum number of signature check operations in an IsStandard() P2SH script */
static const unsigned int MAX_P2SH_SIGOPS = 15;
/** Default for -maxmempool, maximum megabytes of mempool memory usage */
static const unsigned int DEFAULT_MAX_MEMPOOL_SIZE = 300;
/** Default for -permitbaremultisig */
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
extern bool fIsBareMultisigStd;
/** Min feerate for defining dust. Historically this has been based on the
 * minRelayTxFee, however changing the dust limit changes which transactions are
 * standard and should be done with care and ideally rarely. It makes sense to
 * only increase the dust limit after prior releases were already not creating
 * outputs below the new threshold */
static const unsigned int DUST_RELAY_TX_FEE = 30000;

/**
 * Standard script verification flags that standard transactions will comply
 * with. However scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS = MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                         SCRIPT_VERIFY_DERSIG |
                                                         SCRIPT_VERIFY_STRICTENC |
                                                         SCRIPT_VERIFY_MINIMALDATA |
                                                         SCRIPT_VERIFY_NULLDUMMY |
                                                         SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                         SCRIPT_VERIFY_CLEANSTACK |
                                                         SCRIPT_VERIFY_LOW_S |
                                                         SCRIPT_VERIFY_EXCHANGEADDR;

/** For convenience, standard but not mandatory verify flags. */
static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS = STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

// Sanity check the magic numbers when we change them
BOOST_STATIC_ASSERT(DEFAULT_BLOCK_MAX_SIZE <= MAX_BLOCK_SIZE_CURRENT);

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn);

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn);

CAmount GetDustThreshold(const CFeeRate& dustRelayFeeIn);
CAmount GetShieldedDustThreshold(const CFeeRate& dustRelayFeeIn);

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType);

/** Check for standard transaction types
 * @return True if all outputs (scriptPubKeys) use only standard transaction forms
 */
bool IsStandardTx(const CTransactionRef& tx, int nBlockHeight, std::string& reason);

/**
 * Check for standard transaction types
 * @param[in] mapInputs    Map of previous transactions that have outputs we're spending
 * @return True if all inputs (scriptSigs) use only standard transaction forms
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);

extern CFeeRate dustRelayFee;

#endif // PIVX_POLICY_POLICY_H
