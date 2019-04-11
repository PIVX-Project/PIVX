// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2015-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include "main.h"
#include "stakeinput.h"


// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 60;
static const unsigned int MODIFIER_INTERVAL_TESTNET = 60;
extern unsigned int nModifierInterval;
extern unsigned int getIntervalVersion(bool fTestNet);

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool GetKernelStakeModifier(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

bool CheckStake(const CDataStream& ssUniqueID, CAmount nValueIn, const uint64_t nStakeModifier, const uint256& bnTarget, unsigned int nTimeBlockFrom, unsigned int& nTimeTx, uint256& hashProofOfStake);
bool stakeTargetHit(const uint256& hashProofOfStake, const int64_t& nValueIn, const uint256& bnTargetPerCoinDay);
bool Stake(CStakeInput* stakeInput, unsigned int nBits, unsigned int nTimeBlockFrom, unsigned int& nTimeTx, uint256& hashProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlock block, uint256& hashProofOfStake, std::unique_ptr<CStakeInput>& stake);

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

// Get time weight using supplied timestamps
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

#endif // BITCOIN_KERNEL_H
