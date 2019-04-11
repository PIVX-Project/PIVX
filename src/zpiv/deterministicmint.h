// Copyright (c) 2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_DETERMINISTICMINT_H
#define PIVX_DETERMINISTICMINT_H

#include <libzerocoin/Denominations.h>
#include <uint256.h>
#include <serialize.h>

//struct that is safe to store essential mint data, without holding any information that allows for actual spending (serial, randomness, private key)
class CDeterministicMint
{
private:
    uint8_t nVersion;
    uint32_t nCount;
    uint256 hashSeed;
    uint256 hashSerial;
    uint256 hashStake;
    uint256 hashPubcoin;
    uint256 txid;
    int nHeight;
    libzerocoin::CoinDenomination denom;
    bool isUsed;

public:
    CDeterministicMint();
    CDeterministicMint(uint8_t nVersion, const uint32_t& nCount, const uint256& hashSeed, const uint256& hashSerial, const uint256& hashPubcoin, const uint256& hashStake);

    libzerocoin::CoinDenomination GetDenomination() const { return denom; }
    uint32_t GetCount() const { return nCount; }
    int GetHeight() const { return nHeight; }
    uint256 GetSeedHash() const { return hashSeed; }
    uint256 GetSerialHash() const { return hashSerial; }
    uint256 GetStakeHash() const { return hashStake; }
    uint256 GetPubcoinHash() const { return hashPubcoin; }
    uint256 GetTxHash() const { return txid; }
    uint8_t GetVersion() const { return nVersion; }
    bool IsUsed() const { return isUsed; }
    void SetDenomination(const libzerocoin::CoinDenomination denom) { this->denom = denom; }
    void SetHeight(const int& nHeight) { this->nHeight = nHeight; }
    void SetNull();
    void SetStakeHash(const uint256& hashStake) { this->hashStake = hashStake; }
    void SetTxHash(const uint256& txid) { this->txid = txid; }
    void SetUsed(const bool isUsed) { this->isUsed = isUsed; }
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        READWRITE(nCount);
        READWRITE(hashSeed);
        READWRITE(hashSerial);
        READWRITE(hashStake);
        READWRITE(hashPubcoin);
        READWRITE(txid);
        READWRITE(nHeight);
        READWRITE(denom);
        READWRITE(isUsed);
    };
};

#endif //PIVX_DETERMINISTICMINT_H
