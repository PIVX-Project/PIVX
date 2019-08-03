// Copyright (c) 2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <libzerocoin/Coin.h>
#include <tinyformat.h>
#include "deterministicmint.h"


CDeterministicMint::CDeterministicMint()
{
    SetNull();
}

CDeterministicMint::CDeterministicMint(uint8_t nVersion, const uint32_t& nCount, const uint256& hashSeed, const uint256& hashSerial, const uint256& hashPubcoin, const uint256& hashStake)
{
    SetNull();
    this->nVersion = nVersion;
    this->nCount = nCount;
    this->hashSeed = hashSeed;
    this->hashSerial = hashSerial;
    this->hashPubcoin = hashPubcoin;
    this->hashStake = hashStake;
}

void CDeterministicMint::SetNull()
{
    nVersion = libzerocoin::PrivateCoin::CURRENT_VERSION;
    nCount = 0;
    hashSeed = 0;
    hashSerial = 0;
    hashStake = 0;
    hashPubcoin = 0;
    txid = 0;
    nHeight = 0;
    denom = libzerocoin::CoinDenomination::ZQ_ERROR;
    isUsed = false;
}

std::string CDeterministicMint::ToString() const
{
    return strprintf(" DeterministicMint:\n   version=%d\n   count=%d\n   hashseed=%s\n   hashSerial=%s\n   hashStake=%s\n   hashPubcoin=%s\n   txid=%s\n   height=%d\n   denom=%d\n   isUsed=%d\n",
    nVersion, nCount, hashSeed.GetHex(), hashSerial.GetHex(), hashStake.GetHex(), hashPubcoin.GetHex(), txid.GetHex(), nHeight, denom, isUsed);
}
