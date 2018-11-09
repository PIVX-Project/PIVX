// Copyright (c) 2017-2018 The PIVX developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "accumulatormap.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include "init.h"
#include "spork.h"
#include "accumulatorcheckpoints.h"
#include "zsynxchain.h"

using namespace libzerocoin;

std::map<uint32_t, CBigNum> mapAccumulatorValues;
std::list<uint256> listAccCheckpointsNoDB;

uint32_t ParseChecksum(uint256 nChecksum, CoinDenomination denomination)
{
    //shift to the beginning bit of this denomination and trim any remaining bits by returning 32 bits only
    int pos = distance(zerocoinDenomList.begin(), find(zerocoinDenomList.begin(), zerocoinDenomList.end(), denomination));
    nChecksum = nChecksum >> (32*((zerocoinDenomList.size() - 1) - pos));
    return nChecksum.Get32();
}

uint32_t GetChecksum(const CBigNum &bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return hash.Get32();
}

// Find the first occurance of a certain accumulator checksum. Return 0 if not found.
int GetChecksumHeight(uint32_t nChecksum, CoinDenomination denomination)
{
    CBlockIndex* pindex = chainActive[Params().Zerocoin_StartHeight()];
    if (!pindex)
        return 0;

    //Search through blocks to find the checksum
    while (pindex) {
        if (ParseChecksum(pindex->nAccumulatorCheckpoint, denomination) == nChecksum)
            return pindex->nHeight;

        //Skip forward in groups of 10 blocks since checkpoints only change every 10 blocks
        if (pindex->nHeight % 10 == 0) {
            if (pindex->nHeight + 10 > chainActive.Height())
                return 0;
            pindex = chainActive[pindex->nHeight + 10];
            continue;
        }

        pindex = chainActive.Next(pindex);
    }

    return 0;
}

bool GetAccumulatorValueFromChecksum(uint32_t nChecksum, bool fMemoryOnly, CBigNum& bnAccValue)
{
    if (mapAccumulatorValues.count(nChecksum)) {
        bnAccValue = mapAccumulatorValues.at(nChecksum);
        return true;
    }

    if (fMemoryOnly)
        return false;

    if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnAccValue)) {
        bnAccValue = 0;
    }

    return true;
}

bool GetAccumulatorValueFromDB(uint256 nCheckpoint, CoinDenomination denom, CBigNum& bnAccValue)
{
    uint32_t nChecksum = ParseChecksum(nCheckpoint, denom);
    return GetAccumulatorValueFromChecksum(nChecksum, false, bnAccValue);
}

void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue)
{
    //Since accumulators are switching at v2, stop databasing v1 because its useless. Only focus on v2.
    if (chainActive.Height() >= Params().Zerocoin_Block_V2_Start()) {
        zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
}

void DatabaseChecksums(AccumulatorMap& mapAccumulators)
{
    uint256 nCheckpoint = 0;
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.GetValue(denom);
        uint32_t nCheckSum = GetChecksum(bnValue);
        AddAccumulatorChecksum(nCheckSum, bnValue);
        nCheckpoint = nCheckpoint << 32 | nCheckSum;
    }
}

bool EraseChecksum(uint32_t nChecksum)
{
    //erase from both memory and database
    mapAccumulatorValues.erase(nChecksum);
    return zerocoinDB->EraseAccumulatorValue(nChecksum);
}

bool EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksumErase = ParseChecksum(nCheckpointErase, denomination);
        uint32_t nChecksumPrevious = ParseChecksum(nCheckpointPrevious, denomination);

        //if the previous checksum is the same, then it should remain in the database and map
        if(nChecksumErase == nChecksumPrevious)
            continue;

        if (!EraseChecksum(nChecksumErase))
            return false;
    }

    return true;
}

bool LoadAccumulatorValuesFromDB(const uint256 nCheckpoint)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);

        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue)) {
            if (!count(listAccCheckpointsNoDB.begin(), listAccCheckpointsNoDB.end(), nCheckpoint))
                listAccCheckpointsNoDB.push_back(nCheckpoint);
            LogPrint("zero", "%s : Missing databased value for checksum %d", __func__, nChecksum);
            return false;
        }
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
    return true;
}

//Erase accumulator checkpoints for a certain block range
bool EraseCheckpoints(int nStartHeight, int nEndHeight)
{
    if (chainActive.Height() < nStartHeight)
        return false;

    nEndHeight = min(chainActive.Height(), nEndHeight);

    CBlockIndex* pindex = chainActive[nStartHeight];
    uint256 nCheckpointPrev = pindex->pprev->nAccumulatorCheckpoint;

    //Keep a list of checkpoints from the previous block so that we don't delete them
    list<uint32_t> listCheckpointsPrev;
    for (auto denom : zerocoinDenomList)
        listCheckpointsPrev.emplace_back(ParseChecksum(nCheckpointPrev, denom));

    while (true) {
        uint256 nCheckpointDelete = pindex->nAccumulatorCheckpoint;

        for (auto denom : zerocoinDenomList) {
            uint32_t nChecksumDelete = ParseChecksum(nCheckpointDelete, denom);
            if (count(listCheckpointsPrev.begin(), listCheckpointsPrev.end(), nCheckpointDelete))
                continue;
            EraseChecksum(nChecksumDelete);
        }
        LogPrintf("%s : erasing checksums for block %d\n", __func__, pindex->nHeight);

        if (pindex->nHeight + 1 <= nEndHeight)
            pindex = chainActive.Next(pindex);
        else
            break;
    }

    return true;
}

bool InitializeAccumulators(const int nHeight, int& nHeightCheckpoint, AccumulatorMap& mapAccumulators)
{
    if (nHeight < Params().Zerocoin_StartHeight())
        return error("%s: height is below zerocoin activated", __func__);

    //On a specific block, a recalculation of the accumulators will be forced
    if (nHeight == Params().Zerocoin_Block_RecalculateAccumulators()) {
        mapAccumulators.Reset();
        if (!mapAccumulators.Load(chainActive[Params().Zerocoin_Block_LastGoodCheckpoint()]->nAccumulatorCheckpoint))
            return error("%s: failed to reset to previous checkpoint when recalculating accumulators", __func__);

        // Erase the checkpoints from the period of time that bad mints were being made
        if (!EraseCheckpoints(Params().Zerocoin_Block_LastGoodCheckpoint() + 1, nHeight))
            return error("%s : failed to erase Checkpoints while recalculating checkpoints", __func__);

        nHeightCheckpoint = Params().Zerocoin_Block_LastGoodCheckpoint();
        return true;
    }

    if (nHeight >= Params().Zerocoin_Block_V2_Start()) {
        //after v2_start, accumulators need to use v2 params
        mapAccumulators.Reset(Params().Zerocoin_Params(false));

        // 20 after v2 start is when the new checkpoints will be in the block, so don't need to load hard checkpoints
        if (nHeight <= Params().Zerocoin_Block_V2_Start() + 20) {
            //Load hard coded checkpointed value
            AccumulatorCheckpoints::Checkpoint checkpoint = AccumulatorCheckpoints::GetClosestCheckpoint(nHeight,
                                                                                                         nHeightCheckpoint);
            if (nHeightCheckpoint < 0)
                return error("%s: failed to load hard-checkpoint for block %s", __func__, nHeight);

            mapAccumulators.Load(checkpoint);
            return true;
        }
    }

    //Use the previous block's checkpoint to initialize the accumulator's state
    uint256 nCheckpointPrev = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
    if (nCheckpointPrev == 0)
        mapAccumulators.Reset();
    else if (!mapAccumulators.Load(nCheckpointPrev))
        return error("%s: failed to reset to previous checkpoint", __func__);

    nHeightCheckpoint = nHeight;
    return true;
}

//Get checkpoint value for a specific block height
bool CalculateAccumulatorCheckpoint(int nHeight, uint256& nCheckpoint, AccumulatorMap& mapAccumulators)
{
    if (nHeight < Params().Zerocoin_Block_V2_Start()) {
        nCheckpoint = 0;
        return true;
    }

    //the checkpoint is updated every ten blocks, return current active checkpoint if not update block
    if (nHeight % 10 != 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
        return true;
    }

    //set the accumulators to last checkpoint value
    int nHeightCheckpoint;
    mapAccumulators.Reset();
    if (!InitializeAccumulators(nHeight, nHeightCheckpoint, mapAccumulators))
        return error("%s: failed to initialize accumulators", __func__);

    //Whether this should filter out invalid/fraudulent outpoints
    bool fFilterInvalid = nHeight >= Params().Zerocoin_Block_RecalculateAccumulators();

    //Accumulate all coins over the last ten blocks that havent been accumulated (height - 20 through height - 11)
    int nTotalMintsFound = 0;
    CBlockIndex *pindex = chainActive[nHeightCheckpoint - 20];

    while (pindex->nHeight < nHeight - 10) {
        // checking whether we should stop this process due to a shutdown request
        if (ShutdownRequested())
            return false;

        //make sure this block is eligible for accumulation
        if (pindex->nHeight < Params().Zerocoin_StartHeight()) {
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex))
            return error("%s: failed to read block from disk", __func__);

        std::list<PublicCoin> listPubcoins;
        if (!BlockToPubcoinList(block, listPubcoins, fFilterInvalid))
            return error("%s: failed to get zerocoin mintlist from block %d", __func__, pindex->nHeight);

        nTotalMintsFound += listPubcoins.size();
        LogPrint("zero", "%s found %d mints\n", __func__, listPubcoins.size());

        //add the pubcoins to accumulator
        for (const PublicCoin& pubcoin : listPubcoins) {
            if(!mapAccumulators.Accumulate(pubcoin, true))
                return error("%s: failed to add pubcoin to accumulator at height %d", __func__, pindex->nHeight);
        }
        pindex = chainActive.Next(pindex);
    }

    // if there were no new mints found, the accumulator checkpoint will be the same as the last checkpoint
    if (nTotalMintsFound == 0)
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
    else
        nCheckpoint = mapAccumulators.GetCheckpoint();

    LogPrint("zero", "%s checkpoint=%s\n", __func__, nCheckpoint.GetHex());
    return true;
}

bool InvalidCheckpointRange(int nHeight)
{
    return nHeight > Params().Zerocoin_Block_LastGoodCheckpoint() && nHeight < Params().Zerocoin_Block_RecalculateAccumulators();
}

bool ValidateAccumulatorCheckpoint(const CBlock& block, CBlockIndex* pindex, AccumulatorMap& mapAccumulators)
{
    //V1 accumulators are completely phased out by the time this code hits the public and begins generating new checkpoints
    //It is VERY IMPORTANT that when this is being run and height < v2_start, then zSYNX need to be disabled at the same time!!
    if (pindex->nHeight < Params().Zerocoin_Block_V2_Start() || fVerifyingBlocks)
        return true;

    if (pindex->nHeight % 10 == 0) {
        uint256 nCheckpointCalculated = 0;

        if (!CalculateAccumulatorCheckpoint(pindex->nHeight, nCheckpointCalculated, mapAccumulators))
            return error("%s : failed to calculate accumulator checkpoint", __func__);

        if (nCheckpointCalculated != block.nAccumulatorCheckpoint) {
            LogPrintf("%s: block=%d calculated: %s\n block: %s\n", __func__, pindex->nHeight, nCheckpointCalculated.GetHex(), block.nAccumulatorCheckpoint.GetHex());
            return error("%s : accumulator does not match calculated value", __func__);
        }

        return true;
    }

    if (block.nAccumulatorCheckpoint != pindex->pprev->nAccumulatorCheckpoint)
        return error("%s : new accumulator checkpoint generated on a block that is not multiple of 10", __func__);

    return true;
}

void RandomizeSecurityLevel(int& nSecurityLevel)
{
    //security level: this is an important prevention of tracing the coins via timing. Security level represents how many checkpoints
    //of accumulated coins are added *beyond* the checkpoint that the mint being spent was added too. If each spend added the exact same
    //amounts of checkpoints after the mint was accumulated, then you could know the range of blocks that the mint originated from.
    if (nSecurityLevel < 100) {
        //add some randomness to the user's selection so that it is not always the same
        nSecurityLevel += CBigNum::randBignum(10).getint();

        //security level 100 represents adding all available coins that have been accumulated - user did not select this
        if (nSecurityLevel >= 100)
            nSecurityLevel = 99;
    }
}

//Compute how many coins were added to an accumulator up to the end height
int ComputeAccumulatedCoins(int nHeightEnd, libzerocoin::CoinDenomination denom)
{
    CBlockIndex* pindex = chainActive[GetZerocoinStartHeight()];
    int n = 0;
    while (pindex->nHeight < nHeightEnd) {
        n += count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), denom);
        pindex = chainActive.Next(pindex);
    }

    return n;
}

int AddBlockMintsToAccumulator(const libzerocoin::PublicCoin& coin, const int nHeightMintAdded, const CBlockIndex* pindex,
                           libzerocoin::Accumulator* accumulator, bool isWitness)
{
    // if this block contains mints of the denomination that is being spent, then add them to the witness
    int nMintsAdded = 0;
    if (pindex->MintedDenomination(coin.getDenomination())) {
        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex))
            return error("%s: failed to read block from disk while adding pubcoins to witness", __func__);

        list<PublicCoin> listPubcoins;
        if(!BlockToPubcoinList(block, listPubcoins, true))
            return error("%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);

        //add the mints to the witness
        for (const PublicCoin& pubcoin : listPubcoins) {
            if (pubcoin.getDenomination() != coin.getDenomination())
                continue;

            if (isWitness && pindex->nHeight == nHeightMintAdded && pubcoin.getValue() == coin.getValue())
                continue;

            accumulator->increment(pubcoin.getValue());
            ++nMintsAdded;
        }
    }

    return nMintsAdded;
}

bool GetAccumulatorValue(int& nHeight, const libzerocoin::CoinDenomination denom, CBigNum& bnAccValue)
{
    if (nHeight > chainActive.Height())
        return error("%s: height %d is more than active chain height", __func__, nHeight);

    //Every situation except for about 20 blocks should use this method
    uint256 nCheckpointBeforeMint = chainActive[nHeight]->nAccumulatorCheckpoint;
    if (nHeight > Params().Zerocoin_Block_V2_Start() + 20)
        return GetAccumulatorValueFromDB(nCheckpointBeforeMint, denom, bnAccValue);

    int nHeightCheckpoint = 0;
    AccumulatorCheckpoints::Checkpoint checkpoint = AccumulatorCheckpoints::GetClosestCheckpoint(nHeight, nHeightCheckpoint);
    if (nHeightCheckpoint < 0) {
        //Start at the first zerocoin
        libzerocoin::Accumulator accumulator(Params().Zerocoin_Params(false), denom);
        bnAccValue = accumulator.getValue();
        nHeight = Params().Zerocoin_StartHeight() + 10;
        return true;
    }

    nHeight = nHeightCheckpoint;
    bnAccValue = checkpoint.at(denom);

    return true;
}

bool GenerateAccumulatorWitness(const PublicCoin &coin, Accumulator& accumulator, AccumulatorWitness& witness, int nSecurityLevel, int& nMintsAdded, string& strError, CBlockIndex* pindexCheckpoint)
{
    LogPrint("zero", "%s: generating\n", __func__);
    int nLockAttempts = 0;
    while (nLockAttempts < 100) {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain) {
            MilliSleep(50);
            nLockAttempts++;
            continue;
        }
        break;
    }
    if (nLockAttempts == 100)
        return error("%s: could not get lock on cs_main", __func__);
    LogPrint("zero", "%s: after lock\n", __func__);
    uint256 txid;
    if (!zerocoinDB->ReadCoinMint(coin.getValue(), txid))
        return error("%s failed to read mint from db", __func__);

    CTransaction txMinted;
    uint256 hashBlock;
    if (!GetTransaction(txid, txMinted, hashBlock))
        return error("%s failed to read tx", __func__);

    int nHeightTest;
    if (!IsTransactionInChain(txid, nHeightTest))
        return error("%s: mint tx %s is not in chain", __func__, txid.GetHex());

    int nHeightMintAdded = mapBlockIndex[hashBlock]->nHeight;

    //get the checkpoint added at the next multiple of 10
    int nHeightCheckpoint = nHeightMintAdded + (10 - (nHeightMintAdded % 10));

    //the height to start accumulating coins to add to witness
    int nAccStartHeight = nHeightMintAdded - (nHeightMintAdded % 10);

    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    CBigNum bnAccValue = 0;
    if (GetAccumulatorValue(nHeightCheckpoint, coin.getDenomination(), bnAccValue)) {
            accumulator.setValue(bnAccValue);
            witness.resetValue(accumulator, coin);
    }

    //add the pubcoins from the blockchain up to the next checksum starting from the block
    CBlockIndex* pindex = chainActive[nHeightCheckpoint - 10];
    int nChainHeight = chainActive.Height();
    int nHeightStop = nChainHeight % 10;
    nHeightStop = nChainHeight - nHeightStop - 20; // at least two checkpoints deep

    //If looking for a specific checkpoint
    if (pindexCheckpoint)
        nHeightStop = pindexCheckpoint->nHeight - 10;

    //Iterate through the chain and calculate the witness
    int nCheckpointsAdded = 0;
    nMintsAdded = 0;
    RandomizeSecurityLevel(nSecurityLevel); //make security level not always the same and predictable
    libzerocoin::Accumulator witnessAccumulator = accumulator;

    bool fDoubleCounted = false;
    while (pindex) {
        if (pindex->nHeight != nAccStartHeight && pindex->pprev->nAccumulatorCheckpoint != pindex->nAccumulatorCheckpoint)
            ++nCheckpointsAdded;

        //If the security level is satisfied, or the stop height is reached, then initialize the accumulator from here
        bool fSecurityLevelSatisfied = (nSecurityLevel != 100 && nCheckpointsAdded >= nSecurityLevel);
        if (pindex->nHeight >= nHeightStop || fSecurityLevelSatisfied) {
            //If this height is within the invalid range (when fraudulent coins were being minted), then continue past this range
            if(InvalidCheckpointRange(pindex->nHeight))
                continue;

            bnAccValue = 0;
            uint256 nCheckpointSpend = chainActive[pindex->nHeight + 10]->nAccumulatorCheckpoint;
            if (!GetAccumulatorValueFromDB(nCheckpointSpend, coin.getDenomination(), bnAccValue) || bnAccValue == 0)
                return error("%s : failed to find checksum in database for accumulator", __func__);

            accumulator.setValue(bnAccValue);
            break;
        }

        nMintsAdded += AddBlockMintsToAccumulator(coin, nHeightMintAdded, pindex, &witnessAccumulator, true);

        // 10 blocks were accumulated twice when zSYNX v2 was activated
        if (pindex->nHeight == 1050010 && !fDoubleCounted) {
            pindex = chainActive[1050000];
            fDoubleCounted = true;
            continue;
        }

        pindex = chainActive.Next(pindex);
    }

    witness.resetValue(witnessAccumulator, coin);
    if (!witness.VerifyWitness(accumulator, coin))
        return error("%s: failed to verify witness", __func__);

    // A certain amount of accumulated coins are required
    if (nMintsAdded < Params().Zerocoin_RequiredAccumulation()) {
        strError = _(strprintf("Less than %d mints added, unable to create spend", Params().Zerocoin_RequiredAccumulation()).c_str());
        return error("%s : %s", __func__, strError);
    }

    // calculate how many mints of this denomination existed in the accumulator we initialized
    nMintsAdded += ComputeAccumulatedCoins(nAccStartHeight, coin.getDenomination());
    LogPrint("zero", "%s : %d mints added to witness\n", __func__, nMintsAdded);

    return true;
}

map<CoinDenomination, int> GetMintMaturityHeight()
{
    map<CoinDenomination, pair<int, int > > mapDenomMaturity;
    for (auto denom : libzerocoin::zerocoinDenomList)
        mapDenomMaturity.insert(make_pair(denom, make_pair(0, 0)));

    int nConfirmedHeight = chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations();

    // A mint need to get to at least the min maturity height before it will spend.
    int nMinimumMaturityHeight = nConfirmedHeight - (nConfirmedHeight % 10);
    CBlockIndex* pindex = chainActive[nConfirmedHeight];

    while (pindex && pindex->nHeight > Params().Zerocoin_StartHeight()) {
        bool isFinished = true;
        for (auto denom : libzerocoin::zerocoinDenomList) {
            //If the denom has not already had a mint added to it, then see if it has a mint added on this block
            if (mapDenomMaturity.at(denom).first < Params().Zerocoin_RequiredAccumulation()) {
                mapDenomMaturity.at(denom).first += count(pindex->vMintDenominationsInBlock.begin(),
                                                          pindex->vMintDenominationsInBlock.end(), denom);

                //if mint was found then record this block as the first block that maturity occurs.
                if (mapDenomMaturity.at(denom).first >= Params().Zerocoin_RequiredAccumulation())
                    mapDenomMaturity.at(denom).second = std::min(pindex->nHeight, nMinimumMaturityHeight);

                //Signal that we are finished
                isFinished = false;
            }
        }

        if (isFinished)
            break;
        pindex = chainActive[pindex->nHeight - 1];
    }

    //Generate final map
    map<CoinDenomination, int> mapRet;
    for (auto denom : libzerocoin::zerocoinDenomList)
        mapRet.insert(make_pair(denom, mapDenomMaturity.at(denom).second));

    return mapRet;
}
