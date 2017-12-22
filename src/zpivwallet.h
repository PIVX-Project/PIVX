// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ZPIVWALLET_H
#define PIVX_ZPIVWALLET_H

#include <map>
#include "libzerocoin/Coin.h"
#include "mintpool.h"
#include "uint256.h"
#include "primitives/zerocoin.h"

class CzPIVWallet
{
private:
    uint256 seedMaster;
    uint32_t nCount;
    std::string strWalletFile;
    bool fFirstRun;
    const uint8_t nVersion = 1;
    CMintPool mintPool;

public:
    CzPIVWallet(std::string strWalletFile, bool fFirstRun);
    bool SetMasterSeed(const uint256& seedMaster, bool fResetCount = false);
    uint256 GetMasterSeed() { return seedMaster; }
    void SyncWithChain();
    void GenerateDeterministicZPIV(libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin, bool fGenerateOnly = false);
    void GenerateMintPool();
    bool SetMintSeen(CZerocoinMint mint);
    bool IsInMintPool(const CBigNum& bnValue) { return mintPool.Has(bnValue); }

private:
    uint512 GetNextZerocoinSeed();
    uint512 GetZerocoinSeed(uint32_t n);
    void UpdateCount();
    void SeedToZPIV(uint512 seed, CBigNum& bnSerial, CBigNum& bnRandomness);
};

#endif //PIVX_ZPIVWALLET_H
