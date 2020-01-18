// Copyright (c) 2017-2017 The Bitcoin Core developers
// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/consensus.h"
#include "main.h"
#include "script/interpreter.h"
#include "zpivchain.h"

class CZerocoinDB;

//! zerocoin or related verification/tests
bool CheckZerocoinMint(const uint256& txHash, const CTxOut& txout, CValidationState& state, bool fCheckOnly = false);
bool ContextualCheckZerocoinMint(const CTransaction& tx, const libzerocoin::PublicCoin& coin, const CBlockIndex* pindex); // note this function is local only to main.cpp
bool ContextualCheckZerocoinSpend(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashBlock);
bool ContextualCheckZerocoinSpendNoSerialCheck(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashblock);
bool CheckZerocoinSpend(const CTransaction& tx, bool fVerifySignature, CValidationState& state, bool fFakeSerialAttack = false);
bool isBlockBetweenFakeSerialAttackRange(int nHeight);
bool CheckPublicCoinSpendEnforced(int blockHeight, bool isPublicSpend);
bool CheckPublicCoinSpendVersion(int version);
int  CurrentPublicCoinSpendVersion();
void AddWrappedSerialsInflation();
void RecalculateZPIVMinted();
void RecalculateZPIVSpent();
bool RecalculatePIVSupply(int nHeightStart);
bool ReindexAccumulators(std::list<uint256>& listMissingCheckpoints, std::string& strError);
bool UpdateZPIVSupply(const CBlock& block, CBlockIndex* pindex, bool fJustCheck);
