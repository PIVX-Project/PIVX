// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_CONSENSUS_ZEROCOIN_VERIFY_H
#define hemis_CONSENSUS_ZEROCOIN_VERIFY_H

#include "consensus/consensus.h"
#include "script/interpreter.h"

class CValidationState;
class CBigNum;

namespace Consensus {
    struct Params;
}
namespace libzerocoin {
    class CoinSpend;
}

// Fake Serial attack Range
bool isBlockBetweenFakeSerialAttackRange(int nHeight);
// Public coin spend
bool CheckPublicCoinSpendEnforced(int blockHeight, bool isPublicSpend);
bool ContextualCheckZerocoinTx(const CTransactionRef& tx, CValidationState& state, const Consensus::Params& consensus, int nHeight, bool isMined);
bool ContextualCheckZerocoinSpend(const CTransaction& tx, const libzerocoin::CoinSpend* spend, int nHeight);
bool ContextualCheckZerocoinSpendNoSerialCheck(const CTransaction& tx, const libzerocoin::CoinSpend* spend, int nHeight);

bool IsSerialInBlockchain(const CBigNum& bnSerial, int& nHeightTx);

// Returns false if coin spend is invalid. Invalidity/DoS causes are treated inside the function.
bool ParseAndValidateZerocoinSpends(const Consensus::Params& consensus,
                                    const CTransaction& tx, int chainHeight,
                                    CValidationState& state,
                                    std::vector<std::pair<CBigNum, uint256>>& vSpendsRet);

#endif //hemis_CONSENSUS_ZEROCOIN_VERIFY_H
