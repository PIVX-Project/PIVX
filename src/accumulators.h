// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ACCUMULATORS_H
#define PIVX_ACCUMULATORS_H

#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "primitives/zerocoin.h"
#include "uint256.h"

class CAccumulators
{
public:
    static CAccumulators& getInstance()
    {
        static CAccumulators instance;
        return instance;
    }
private:
    std::map<libzerocoin::CoinDenomination, std::unique_ptr<libzerocoin::Accumulator> > mapAccumulators;
    std::map<CBigNum, uint256> mapSerials;
    std::map<uint32_t, CBigNum> mapAccumulatorValues;
    std::list<uint256> listAccCheckpointsNoDB;

    CAccumulators() { Setup(); }
    void Setup();

public:
    CAccumulators(CAccumulators const&) = delete;
    void operator=(CAccumulators const&) = delete;

    libzerocoin::Accumulator Get(libzerocoin::CoinDenomination denomination);
    bool AddPubCoinToAccumulator(const libzerocoin::PublicCoin& publicCoin);
    bool IntializeWitnessAndAccumulator(const libzerocoin::PublicCoin &coin, libzerocoin::Accumulator& accumulator, libzerocoin::AccumulatorWitness& witness, int nSecurityLevel, int& nMintsAdded, std::string& strError);
    bool EraseCoinSpend(const CBigNum& bnSerial);
    bool EraseCoinMint(const CBigNum& bnPubCoin);

    //checksum/checkpoint
    void DatabaseChecksums(const uint256& nCheckpoint);
    void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly = false);
    bool LoadAccumulatorValuesFromDB(const uint256 nCheckpoint);
    bool EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious);
    uint32_t GetChecksum(const CBigNum &bnValue);
    uint32_t GetChecksum(const libzerocoin::Accumulator &accumulator);
    uint256 GetCheckpoint();
    bool GetCheckpoint(int nHeight, uint256& nCheckpoint);
    CBigNum GetAccumulatorValueFromChecksum(const uint32_t& nChecksum);
    CBigNum GetAccumulatorValueFromCheckpoint(const uint256& nCheckpoint, libzerocoin::CoinDenomination denomination);
    bool ResetToCheckpoint(const uint256& nCheckpoint);
    std::list<uint256> GetAccCheckpointsNoDB() { return listAccCheckpointsNoDB; };
    void ClearAccCheckpointsNoDB() { listAccCheckpointsNoDB.clear(); }
};

uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination);


#endif //PIVX_ACCUMULATORS_H
