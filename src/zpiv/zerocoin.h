// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ZEROCOIN_H
#define PIVX_ZEROCOIN_H

#include <amount.h>
#include <limits.h>
#include <chainparams.h>
#include "libzerocoin/bignum.h"
#include "libzerocoin/Denominations.h"
#include "key.h"
#include "serialize.h"

//struct that is safe to store essential mint data, without holding any information that allows for actual spending (serial, randomness, private key)
struct CMintMeta
{
    int nHeight;
    uint256 hashSerial;
    uint256 hashPubcoin;
    uint256 hashStake; //requires different hashing method than hashSerial above
    uint8_t nVersion;
    libzerocoin::CoinDenomination denom;
    uint256 txid;
    bool isUsed;
    bool isArchived;
    bool isDeterministic;
    bool isSeedCorrect;

    bool operator <(const CMintMeta& a) const;
};

uint256 GetSerialHash(const CBigNum& bnSerial);
uint256 GetPubCoinHash(const CBigNum& bnValue);

class CZerocoinMint
{
private:
    libzerocoin::CoinDenomination denomination;
    int nHeight;
    CBigNum value;
    CBigNum randomness;
    CBigNum serialNumber;
    uint256 txid;
    CPrivKey privkey;
    uint8_t version;
    bool isUsed;

public:
    static const int STAKABLE_VERSION = 2;
    static const int CURRENT_VERSION = 2;

    CZerocoinMint()
    {
        SetNull();
    }

    CZerocoinMint(libzerocoin::CoinDenomination denom, const CBigNum& value, const CBigNum& randomness, const CBigNum& serialNumber, bool isUsed, const uint8_t& nVersion, CPrivKey* privkey = nullptr)
    {
        SetNull();
        this->denomination = denom;
        this->value = value;
        this->randomness = randomness;
        this->serialNumber = serialNumber;
        this->isUsed = isUsed;
        this->version = nVersion;
        if (nVersion >= 2 && privkey)
            this->privkey = *privkey;
    }

    void SetNull()
    {
        isUsed = false;
        randomness = 0;
        value = 0;
        denomination = libzerocoin::ZQ_ERROR;
        nHeight = 0;
        txid = 0;
        version = 1;
        privkey.clear();
    }

    uint256 GetHash() const;

    CBigNum GetValue() const { return value; }
    void SetValue(CBigNum value){ this->value = value; }
    libzerocoin::CoinDenomination GetDenomination() const { return denomination; }
    int64_t GetDenominationAsAmount() const { return denomination * COIN; }
    void SetDenomination(libzerocoin::CoinDenomination denom){ this->denomination = denom; }
    int GetHeight() const { return nHeight; }
    void SetHeight(int nHeight){ this->nHeight = nHeight; }
    bool IsUsed() const { return this->isUsed; }
    void SetUsed(bool isUsed){ this->isUsed = isUsed; }
    CBigNum GetRandomness() const{ return randomness; }
    void SetRandomness(CBigNum rand){ this->randomness = rand; }
    CBigNum GetSerialNumber() const { return serialNumber; }
    void SetSerialNumber(CBigNum serial){ this->serialNumber = serial; }
    uint256 GetTxHash() const { return this->txid; }
    void SetTxHash(uint256 txid) { this->txid = txid; }
    uint8_t GetVersion() const { return this->version; }
    void SetVersion(const uint8_t nVersion) { this->version = nVersion; }
    CPrivKey GetPrivKey() const { return this->privkey; }
    void SetPrivKey(const CPrivKey& privkey) { this->privkey = privkey; }
    bool GetKeyPair(CKey& key) const;

    inline bool operator <(const CZerocoinMint& a) const { return GetHeight() < a.GetHeight(); }

    CZerocoinMint(const CZerocoinMint& other) {
        denomination = other.GetDenomination();
        nHeight = other.GetHeight();
        value = other.GetValue();
        randomness = other.GetRandomness();
        serialNumber = other.GetSerialNumber();
        txid = other.GetTxHash();
        isUsed = other.IsUsed();
        version = other.GetVersion();
        privkey = other.privkey;
    }

    std::string ToString() const;

    bool operator == (const CZerocoinMint& other) const
    {
        return this->GetValue() == other.GetValue();
    }
    
    // Copy another CZerocoinMint
    inline CZerocoinMint& operator=(const CZerocoinMint& other) {
        denomination = other.GetDenomination();
        nHeight = other.GetHeight();
        value = other.GetValue();
        randomness = other.GetRandomness();
        serialNumber = other.GetSerialNumber();
        txid = other.GetTxHash();
        isUsed = other.IsUsed();
        version = other.GetVersion();
        privkey = other.GetPrivKey();
        return *this;
    }
    
    // why 6 below (SPOCK)
    inline bool checkUnused(int denom, int Height) const {
        if (IsUsed() == false && GetDenomination() == denomination && GetRandomness() != 0 && GetSerialNumber() != 0 && GetHeight() != -1 && GetHeight() != INT_MAX && GetHeight() >= 1 && (GetHeight() + 6 <= Height)) {
            return true;
        } else {
            return false;
        }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(isUsed);
        READWRITE(randomness);
        READWRITE(serialNumber);
        READWRITE(value);
        READWRITE(denomination);
        READWRITE(nHeight);
        READWRITE(txid);

        bool fVersionedMint = true;
        try {
            READWRITE(version);
        } catch (...) {
            fVersionedMint = false;
        }

        if (version > CURRENT_VERSION) {
            version = 1;
            fVersionedMint = false;
        }

        if (fVersionedMint)
            READWRITE(privkey);
    };
};

class CZerocoinSpend
{
private:
    CBigNum coinSerial;
    uint256 hashTx;
    CBigNum pubCoin;
    libzerocoin::CoinDenomination denomination;
    unsigned int nAccumulatorChecksum;
    int nMintCount; //memory only - the amount of mints that belong to the accumulator this is spent from

public:
    CZerocoinSpend()
    {
        SetNull();
    }

    CZerocoinSpend(CBigNum coinSerial, uint256 hashTx, CBigNum pubCoin, libzerocoin::CoinDenomination denomination, unsigned int nAccumulatorChecksum)
    {
        this->coinSerial = coinSerial;
        this->hashTx = hashTx;
        this->pubCoin = pubCoin;
        this->denomination = denomination;
        this->nAccumulatorChecksum = nAccumulatorChecksum;
    }

    void SetNull()
    {
        coinSerial = 0;
        hashTx = 0;
        pubCoin = 0;
        denomination = libzerocoin::ZQ_ERROR;
    }

    CBigNum GetSerial() const { return coinSerial; }
    uint256 GetTxHash() const { return hashTx; }
    void SetTxHash(uint256 hash) { this->hashTx = hash; }
    CBigNum GetPubCoin() const { return pubCoin; }
    libzerocoin::CoinDenomination GetDenomination() const { return denomination; }
    unsigned int GetAccumulatorChecksum() const { return this->nAccumulatorChecksum; }
    uint256 GetHash() const;
    void SetMintCount(int nMintsAdded) { this->nMintCount = nMintsAdded; }
    int GetMintCount() const { return nMintCount; }
 
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(coinSerial);
        READWRITE(hashTx);
        READWRITE(pubCoin);
        READWRITE(denomination);
        READWRITE(nAccumulatorChecksum);
    };
};

class CZerocoinSpendReceipt
{
private:
    std::string strStatusMessage;
    int nStatus;
    int nNeededSpends;
    std::vector<CZerocoinSpend> vSpends;

public:
    void AddSpend(const CZerocoinSpend& spend);
    std::vector<CZerocoinSpend> GetSpends();
    void SetStatus(std::string strStatus, int nStatus, int nNeededSpends = 0);
    std::string GetStatusMessage();
    int GetStatus();
    int GetNeededSpends();
};

/**
 * Wrapped serials attack inflation, only for mainnet.
 * FUTURE: Move this to another file..
 * @param denom
 * @return
 */
int GetWrapppedSerialInflation(libzerocoin::CoinDenomination denom);

int64_t GetWrapppedSerialInflationAmount();

#endif //PIVX_ZEROCOIN_H
