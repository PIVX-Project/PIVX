// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_KERNEL_H
#define hemis_KERNEL_H

#include "stakeinput.h"

class CStakeKernel {
public:
    /**
     * CStakeKernel Constructor
     *
     * @param[in]   pindexPrev      index of the parent of the kernel block
     * @param[in]   stakeInput      input for the coinstake of the kernel block
     * @param[in]   nBits           target difficulty bits of the kernel block
     * @param[in]   nTimeTx         time of the kernel block
     */
    CStakeKernel(const CBlockIndex* const pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx);

    // Return stake kernel hash
    uint256 GetHash() const;

    // Check that the kernel hash meets the target required
    bool CheckKernelHash(bool fSkipLog = false) const;

private:
    // kernel message hashed
    CDataStream stakeModifier{CDataStream(SER_GETHASH, 0)};
    int nTimeBlockFrom{0};
    CDataStream stakeUniqueness{CDataStream(SER_GETHASH, 0)};
    int nTime{0};
    // hash target
    unsigned int nBits{0};     // difficulty for the target
    CAmount stakeValue{0};     // target multiplier
};

/* PoS Validation */

/*
 * Stake                Check if stakeInput can stake a block on top of pindexPrev
 *
 * @param[in]   pindexPrev      index of the parent block of the block being staked
 * @param[in]   stakeInput      input for the coinstake
 * @param[in]   nBits           target difficulty bits
 * @param[in]   nTimeTx         new blocktime
 * @return      bool            true if stake kernel hash meets target protocol
 */
bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx);

/*
 * CheckProofOfStake    Check if block has valid proof of stake
 *
 * @param[in]   block           block with the proof being verified
 * @param[out]  strError        string returning error message (if any, else empty)
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            true if the block has a valid proof of stake
 */
bool CheckProofOfStake(const CBlock& block, std::string& strError, const CBlockIndex* pindexPrev = nullptr);

/*
 * GetStakeKernelHash   Return stake kernel of a block
 *
 * @param[out]  hashRet         hash of the kernel (set by this function)
 * @param[in]   block           block with the kernel to return
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            false if kernel cannot be initialized, true otherwise
 */
bool GetStakeKernelHash(uint256& hashRet, const CBlock& block, const CBlockIndex* pindexPrev = nullptr);

#endif // hemis_KERNEL_H
