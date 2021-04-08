// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
//Copyright (c) 2016-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "legacy/stakemodifier.h"  // for ComputeNextStakeModifier


/**
 * CChain implementation
 */
void CChain::SetTip(CBlockIndex* pindex)
{
    if (pindex == NULL) {
        vChain.clear();
        return;
    }
    vChain.resize(pindex->nHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
    }
}

CBlockLocator CChain::GetLocator(const CBlockIndex* pindex) const
{
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->nHeight - nStep, 0);
        if (Contains(pindex)) {
            // Use O(1) CChain index if possible.
            pindex = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            pindex = pindex->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockIndex* CChain::FindFork(const CBlockIndex* pindex) const
{
    if (pindex == nullptr)
        return nullptr;
    if (pindex->nHeight > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

CBlockIndex::CBlockIndex(const CBlock& block):
        nVersion{block.nVersion},
        hashMerkleRoot{block.hashMerkleRoot},
        nTime{block.nTime},
        nBits{block.nBits},
        nNonce{block.nNonce}
{
    if (block.IsProofOfStake())
        SetProofOfStake();
}

std::string CBlockIndex::ToString() const
{
    return strprintf("CBlockIndex(pprev=%p, nHeight=%d, merkle=%s, hashBlock=%s)",
        pprev, nHeight,
        hashMerkleRoot.ToString(),
        GetBlockHash().ToString());
}

CDiskBlockPos CBlockIndex::GetBlockPos() const
{
    CDiskBlockPos ret;
    if (nStatus & BLOCK_HAVE_DATA) {
        ret.nFile = nFile;
        ret.nPos = nDataPos;
    }
    return ret;
}

CDiskBlockPos CBlockIndex::GetUndoPos() const
{
    CDiskBlockPos ret;
    if (nStatus & BLOCK_HAVE_UNDO) {
        ret.nFile = nFile;
        ret.nPos = nUndoPos;
    }
    return ret;
}

CBlockHeader CBlockIndex::GetBlockHeader() const
{
    CBlockHeader block;
    block.nVersion = nVersion;
    if (pprev) block.hashPrevBlock = pprev->GetBlockHash();
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime = nTime;
    block.nBits = nBits;
    block.nNonce = nNonce;
    return block;
}

int64_t CBlockIndex::MaxFutureBlockTime() const
{
    return GetAdjustedTime() + Params().GetConsensus().FutureBlockTimeDrift(nHeight+1);
}

int64_t CBlockIndex::MinPastBlockTime() const
{
    const Consensus::Params& consensus = Params().GetConsensus();
    // Time Protocol v1: pindexPrev->MedianTimePast + 1
    if (!consensus.IsTimeProtocolV2(nHeight+1))
        return GetMedianTimePast();

    // on the transition from Time Protocol v1 to v2
    // pindexPrev->nTime might be in the future (up to the allowed drift)
    // so we allow the nBlockTimeProtocolV2 to be at most (180-14) seconds earlier than previous block
    if (nHeight + 1 == consensus.height_start_TimeProtoV2)
        return GetBlockTime() - consensus.FutureBlockTimeDrift(nHeight) + consensus.FutureBlockTimeDrift(nHeight + 1);

    // Time Protocol v2: pindexPrev->nTime
    return GetBlockTime();
}

enum { nMedianTimeSpan = 11 };

int64_t CBlockIndex::GetMedianTimePast() const
{
    int64_t pmedian[nMedianTimeSpan];
    int64_t* pbegin = &pmedian[nMedianTimeSpan];
    int64_t* pend = &pmedian[nMedianTimeSpan];

    const CBlockIndex* pindex = this;
    for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
        *(--pbegin) = pindex->GetBlockTime();

    std::sort(pbegin, pend);
    return pbegin[(pend - pbegin) / 2];
}

unsigned int CBlockIndex::GetStakeEntropyBit() const
{
    unsigned int nEntropyBit = ((GetBlockHash().GetCheapHash()) & 1);
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("GetStakeEntropyBit: nHeight=%u hashBlock=%s nEntropyBit=%u\n", nHeight, GetBlockHash().ToString().c_str(), nEntropyBit);

    return nEntropyBit;
}

bool CBlockIndex::SetStakeEntropyBit(unsigned int nEntropyBit)
{
    if (nEntropyBit > 1)
        return false;
    nFlags |= (nEntropyBit ? BLOCK_STAKE_ENTROPY : 0);
    return true;
}

// Sets V1 stake modifier (uint64_t)
void CBlockIndex::SetStakeModifier(const uint64_t nStakeModifier, bool fGeneratedStakeModifier)
{
    vStakeModifier.clear();
    const size_t modSize = sizeof(nStakeModifier);
    vStakeModifier.resize(modSize);
    std::memcpy(vStakeModifier.data(), &nStakeModifier, modSize);
    if (fGeneratedStakeModifier)
        nFlags |= BLOCK_STAKE_MODIFIER;

}

// Generates and sets new V1 stake modifier
void CBlockIndex::SetNewStakeModifier()
{
    // compute stake entropy bit for stake modifier
    if (!SetStakeEntropyBit(GetStakeEntropyBit()))
        LogPrintf("%s : SetStakeEntropyBit() failed\n", __func__);
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pprev, nStakeModifier, fGeneratedStakeModifier))
        LogPrintf("%s : ComputeNextStakeModifier() failed \n",  __func__);
    return SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
}

// Sets V2 stake modifiers (uint256)
void CBlockIndex::SetStakeModifier(const uint256& nStakeModifier)
{
    vStakeModifier.clear();
    vStakeModifier.insert(vStakeModifier.begin(), nStakeModifier.begin(), nStakeModifier.end());
}

// Generates and sets new V2 stake modifier
void CBlockIndex::SetNewStakeModifier(const uint256& prevoutId)
{
    // Shouldn't be called on V1 modifier's blocks (or before setting pprev)
    if (nHeight < Params().GetConsensus().height_start_StakeModifierV2) return;
    if (!pprev) throw std::runtime_error(strprintf("%s : ERROR: null pprev", __func__));

    // Generate Hash(prevoutId | prevModifier) - switch with genesis modifier (0) on upgrade block
    CHashWriter ss(SER_GETHASH, 0);
    ss << prevoutId;
    ss << pprev->GetStakeModifierV2();
    SetStakeModifier(ss.GetHash());
}

// Returns V1 stake modifier (uint64_t)
uint64_t CBlockIndex::GetStakeModifierV1() const
{
    if (vStakeModifier.empty() || nHeight >= Params().GetConsensus().height_start_StakeModifierV2)
        return 0;
    uint64_t nStakeModifier;
    std::memcpy(&nStakeModifier, vStakeModifier.data(), vStakeModifier.size());
    return nStakeModifier;
}

// Returns V2 stake modifier (uint256)
uint256 CBlockIndex::GetStakeModifierV2() const
{
    if (vStakeModifier.empty() || nHeight < Params().GetConsensus().height_start_StakeModifierV2)
        return UINT256_ZERO;
    uint256 nStakeModifier;
    std::memcpy(nStakeModifier.begin(), vStakeModifier.data(), vStakeModifier.size());
    return nStakeModifier;
}

//! Check whether this block index entry is valid up to the passed validity level.
bool CBlockIndex::IsValid(enum BlockStatus nUpTo) const
{
    assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
    if (nStatus & BLOCK_FAILED_MASK)
        return false;
    return ((nStatus & BLOCK_VALID_MASK) >= nUpTo);
}

//! Raise the validity level of this block index entry.
//! Returns true if the validity was changed.
bool CBlockIndex::RaiseValidity(enum BlockStatus nUpTo)
{
    assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
    if (nStatus & BLOCK_FAILED_MASK)
        return false;
    if ((nStatus & BLOCK_VALID_MASK) < nUpTo) {
        nStatus = (nStatus & ~BLOCK_VALID_MASK) | nUpTo;
        return true;
    }
    return false;
}
