// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "accumulatormap.h"
#include "chainparams.h"
#include "validation.h"
#include "txdb.h"
#include "init.h"
#include "spork.h"
#include "accumulatorcheckpoints.h"
#include "zpivchain.h"
#include "tinyformat.h"


std::map<uint32_t, CBigNum> mapAccumulatorValues;
std::list<uint256> listAccCheckpointsNoDB;


uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination)
{
    //shift to the beginning bit of this denomination and trim any remaining bits by returning 32 bits only
    int pos = distance(libzerocoin::zerocoinDenomList.begin(), find(libzerocoin::zerocoinDenomList.begin(), libzerocoin::zerocoinDenomList.end(), denomination));
    nChecksum = nChecksum >> (32*((libzerocoin::zerocoinDenomList.size() - 1) - pos));
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
int GetChecksumHeight(uint32_t nChecksum, libzerocoin::CoinDenomination denomination)
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


bool GetAccumulatorValueFromDB(uint256 nCheckpoint, libzerocoin::CoinDenomination denom, CBigNum& bnAccValue)
{
    uint32_t nChecksum = ParseChecksum(nCheckpoint, denom);
    return GetAccumulatorValueFromChecksum(nChecksum, false, bnAccValue);
}


void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue)
{
    //Since accumulators are switching at v2, stop databasing v1 because its useless. Only focus on v2.
    if (chainActive.Height() >= Params().Zerocoin_Block_V2_Start()) {
        zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
        mapAccumulatorValues.insert(std::make_pair(nChecksum, bnValue));
    }
}


void DatabaseChecksums(AccumulatorMap& mapAccumulators)
{
    uint256 nCheckpoint = 0;
    for (auto& denom : libzerocoin::zerocoinDenomList) {
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
    for (auto& denomination : libzerocoin::zerocoinDenomList) {
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
    for (auto& denomination : libzerocoin::zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);

        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue)) {
            if (!count(listAccCheckpointsNoDB.begin(), listAccCheckpointsNoDB.end(), nCheckpoint))
                listAccCheckpointsNoDB.push_back(nCheckpoint);
            LogPrint("zero", "%s : Missing databased value for checksum %d\n", __func__, nChecksum);
            return false;
        }
        mapAccumulatorValues.insert(std::make_pair(nChecksum, bnValue));
    }
    return true;
}


//Erase accumulator checkpoints for a certain block range
bool EraseCheckpoints(int nStartHeight, int nEndHeight)
{
    if (chainActive.Height() < nStartHeight)
        return false;

    nEndHeight = std::min(chainActive.Height(), nEndHeight);

    CBlockIndex* pindex = chainActive[nStartHeight];
    uint256 nCheckpointPrev = pindex->pprev->nAccumulatorCheckpoint;

    //Keep a list of checkpoints from the previous block so that we don't delete them
    std::list<uint32_t> listCheckpointsPrev;
    for (auto denom : libzerocoin::zerocoinDenomList)
        listCheckpointsPrev.emplace_back(ParseChecksum(nCheckpointPrev, denom));

    while (true) {
        uint256 nCheckpointDelete = pindex->nAccumulatorCheckpoint;

        for (auto denom : libzerocoin::zerocoinDenomList) {
            uint32_t nChecksumDelete = ParseChecksum(nCheckpointDelete, denom);
            if (std::count(listCheckpointsPrev.begin(), listCheckpointsPrev.end(), nCheckpointDelete))
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
    if (nHeight == Params().Zerocoin_Block_RecalculateAccumulators() && Params().NetworkID() != CBaseChainParams::REGTEST) {
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
        if (nHeight <= Params().Zerocoin_Block_V2_Start() + 20 && Params().NetworkID() != CBaseChainParams::REGTEST) {
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

        std::list<libzerocoin::PublicCoin> listPubcoins;
        if (!BlockToPubcoinList(block, listPubcoins, fFilterInvalid))
            return error("%s: failed to get zerocoin mintlist from block %d", __func__, pindex->nHeight);

        nTotalMintsFound += listPubcoins.size();
        LogPrint("zero", "%s found %d mints\n", __func__, listPubcoins.size());

        //add the pubcoins to accumulator
        for (const libzerocoin::PublicCoin& pubcoin : listPubcoins) {
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
    //It is VERY IMPORTANT that when this is being run and height < v2_start, then zPIV need to be disabled at the same time!!
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


//########################## Witness


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


std::list<libzerocoin::PublicCoin> GetPubcoinFromBlock(const CBlockIndex* pindex){
    //grab mints from this block
    CBlock block;
    if(!ReadBlockFromDisk(block, pindex))
        throw GetPubcoinException("GetPubcoinFromBlock: failed to read block from disk while adding pubcoins to witness");
    std::list<libzerocoin::PublicCoin> listPubcoins;
    if(!BlockToPubcoinList(block, listPubcoins, true))
        throw GetPubcoinException("GetPubcoinFromBlock: failed to get zerocoin mintlist from block "+std::to_string(pindex->nHeight)+"\n");
    return listPubcoins;
}



int AddBlockMintsToAccumulator(const libzerocoin::CoinDenomination den, const CBloomFilter filter, const CBlockIndex* pindex,
                               libzerocoin::Accumulator* accumulator, bool isWitness, std::list<CBigNum>& notAddedCoins)
{
    // if this block contains mints of the denomination that is being spent, then add them to the witness
    int nMintsAdded = 0;
    if (pindex->MintedDenomination(den)) {
        //add the mints to the witness
        for (const libzerocoin::PublicCoin& pubcoin : GetPubcoinFromBlock(pindex)) {
            if (pubcoin.getDenomination() != den) {
                continue;
            }

            if (isWitness && filter.contains(pubcoin.getValue().getvch())) {
                notAddedCoins.emplace_back(pubcoin.getValue());
                continue;
            }

            accumulator->increment(pubcoin.getValue());
            ++nMintsAdded;
        }
    }

    return nMintsAdded;
}

int AddBlockMintsToAccumulator(const libzerocoin::PublicCoin& coin, const int nHeightMintAdded, const CBlockIndex* pindex,
                               libzerocoin::Accumulator* accumulator, bool isWitness)
{
    // if this block contains mints of the denomination that is being spent, then add them to the witness
    int nMintsAdded = 0;
    if (pindex->MintedDenomination(coin.getDenomination())) {
        //add the mints to the witness
        for (const libzerocoin::PublicCoin& pubcoin : GetPubcoinFromBlock(pindex)) {
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


int AddBlockMintsToAccumulator(CoinWitnessData* coinWitness, const CBlockIndex* pindex, bool isWitness)
{
    // TODO: This should be the witness..
    return AddBlockMintsToAccumulator(
            *coinWitness->coin.get(),
            coinWitness->nHeightMintAdded,
            pindex,
            coinWitness->pAccumulator.get(),
            isWitness
    );
}


bool GetAccumulatorValue(int& nHeight, const libzerocoin::CoinDenomination denom, CBigNum& bnAccValue)
{
    if (nHeight > chainActive.Height())
        return error("%s: height %d is more than active chain height", __func__, nHeight);

    //Every situation except for about 20 blocks should use this method
    uint256 nCheckpointBeforeMint = chainActive[nHeight]->nAccumulatorCheckpoint;
    if (nHeight > Params().Zerocoin_Block_V2_Start() + 20) {
        return GetAccumulatorValueFromDB(nCheckpointBeforeMint, denom, bnAccValue);
    }

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


//############ Witness Generation

/**
 * TODO: Why we are locking the wallet in this way?
 * @return
 */
bool LockMethod(){
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
    // locked
    return true;
}

int SearchMintHeightOf(CBigNum value){
    uint256 txid;
    if (!zerocoinDB->ReadCoinMint(value, txid))
        throw searchMintHeightException("searchForMintHeightOf:: failed to read mint from db");

    CTransaction txMinted;
    uint256 hashBlock;
    if (!GetTransaction(txid, txMinted, hashBlock))
        throw searchMintHeightException("searchForMintHeightOf:: failed to read tx");

    int nHeightTest;
    if (!IsTransactionInChain(txid, nHeightTest))
        throw searchMintHeightException("searchForMintHeightOf:: mint tx "+ txid.GetHex() +" is not in chain");

    return mapBlockIndex[hashBlock]->nHeight;
}


void AccumulateRange(CoinWitnessData* coinWitness, int nHeightEnd)
{
    bool fDoubleCounted = false;
    int64_t nTimeStart = GetTimeMicros();
    int nHeightStart = std::max(coinWitness->nHeightAccStart, coinWitness->nHeightAccEnd + 1);
    CBlockIndex* pindex = chainActive[nHeightStart];

    LogPrint("zero", "%s: start=%d end=%d\n", __func__, nHeightStart, nHeightEnd);
    while (pindex && pindex->nHeight <= nHeightEnd) {
        coinWitness->nMintsAdded += AddBlockMintsToAccumulator(coinWitness, pindex, true);
        coinWitness->nHeightAccEnd = pindex->nHeight;

        // 10 blocks were accumulated twice when zPIV v2 was activated
        if (pindex->nHeight == Params().Zerocoin_Block_Double_Accumulated() + 10 && !fDoubleCounted) {
            pindex = chainActive[Params().Zerocoin_Block_Double_Accumulated()];
            fDoubleCounted = true;
            continue;
        }

        pindex = chainActive.Next(pindex);
    }
    int64_t nTimeEnd = GetTimeMicros();
    LogPrint("bench", "        - Range accumulation completed in %.2fms\n", 0.001 * (nTimeEnd - nTimeStart));
}


bool GenerateAccumulatorWitness(CoinWitnessData* coinWitness, AccumulatorMap& mapAccumulators, CBlockIndex* pindexCheckpoint)
{
    try {
        // Lock
        LogPrint("zero", "%s: generating\n", __func__);
        if (!LockMethod()) return false;
        LogPrint("zero", "%s: after lock\n", __func__);

        int64_t nTimeStart = GetTimeMicros();

        //If there is a Acc End height filled in, then this has already been partially accumulated.
        if (!coinWitness->nHeightAccEnd) {
            LogPrintf("RESET ACC\n");
            coinWitness->pAccumulator = std::unique_ptr<libzerocoin::Accumulator>(new libzerocoin::Accumulator(Params().Zerocoin_Params(false), coinWitness->denom));
            coinWitness->pWitness = std::unique_ptr<libzerocoin::AccumulatorWitness>(new libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(false), *coinWitness->pAccumulator, *coinWitness->coin));
        }

        // Mint added height
        coinWitness->SetHeightMintAdded(SearchMintHeightOf(coinWitness->coin->getValue()));

        // Set the initial state of the witness accumulator for this coin.
        CBigNum bnAccValue = 0;
        if (!coinWitness->nHeightAccEnd && GetAccumulatorValue(coinWitness->nHeightCheckpoint, coinWitness->coin->getDenomination(), bnAccValue)) {
            libzerocoin::Accumulator witnessAccumulator(Params().Zerocoin_Params(false), coinWitness->denom, bnAccValue);
            coinWitness->pAccumulator->setValue(witnessAccumulator.getValue());
        }

        //add the pubcoins from the blockchain up to the next checksum starting from the block
        int nChainHeight = chainActive.Height();
        int nHeightMax = nChainHeight % 10;
        nHeightMax = nChainHeight - nHeightMax - 20; // at least two checkpoints deep

        // Determine the height to stop at
        int nHeightStop;
        if (pindexCheckpoint) {
            nHeightStop = pindexCheckpoint->nHeight - 10;
            nHeightStop -= nHeightStop % 10;
            LogPrint("zero", "%s: using checkpoint height %d\n", __func__, pindexCheckpoint->nHeight);
        } else {
            nHeightStop = nHeightMax;
        }

        if (nHeightStop > coinWitness->nHeightAccEnd)
            AccumulateRange(coinWitness, nHeightStop - 1);

        mapAccumulators.Load(chainActive[nHeightStop + 10]->nAccumulatorCheckpoint);
        coinWitness->pWitness->resetValue(*coinWitness->pAccumulator, *coinWitness->coin);
        if(!coinWitness->pWitness->VerifyWitness(mapAccumulators.GetAccumulator(coinWitness->denom), *coinWitness->coin))
            return error("%s: failed to verify witness", __func__);

        // A certain amount of accumulated coins are required
        if (coinWitness->nMintsAdded < Params().Zerocoin_RequiredAccumulation())
            return error("%s : Less than %d mints added, unable to create spend. %s", __func__, Params().Zerocoin_RequiredAccumulation(), coinWitness->ToString());

        // calculate how many mints of this denomination existed in the accumulator we initialized
        coinWitness->nMintsAdded += ComputeAccumulatedCoins(coinWitness->nHeightAccStart, coinWitness->denom);
        LogPrint("zero", "%s : %d mints added to witness\n", __func__, coinWitness->nMintsAdded);

        int64_t nTime1 = GetTimeMicros();
        LogPrint("bench", "        - Witness generated in %.2fms\n", 0.001 * (nTime1 - nTimeStart));

        return true;

        // TODO: I know that could merge all of this exception but maybe it's not really good.. think if we should have a different treatment for each one
    } catch (searchMintHeightException e) {
        return error("%s: searchMintHeightException: %s", __func__, e.message);
    } catch (ChecksumInDbNotFoundException e) {
        return error("%s: ChecksumInDbNotFoundException: %s", __func__, e.message);
    } catch (GetPubcoinException e) {
        return error("%s: GetPubcoinException: %s", __func__, e.message);
    }
}

bool calculateAccumulatedBlocksFor(
        int startHeight,
        int nHeightStop,
        CBlockIndex *pindex,
        CBigNum &bnAccValue,
        libzerocoin::Accumulator &accumulator,
        libzerocoin::CoinDenomination den,
        CBloomFilter filter,
        libzerocoin::Accumulator &witnessAccumulator,
        std::list<CBigNum>& ret,
        std::string& strError
){
    bool fDoubleCounted = false;
    int nMintsAdded = 0;
    while (pindex) {

        if (pindex->nHeight >= nHeightStop) {
            //If this height is within the invalid range (when fraudulent coins were being minted), then continue past this range
            if(InvalidCheckpointRange(pindex->nHeight))
                continue;

            bnAccValue = 0;
            uint256 nCheckpointSpend = chainActive[pindex->nHeight + 10]->nAccumulatorCheckpoint;
            if (!GetAccumulatorValueFromDB(nCheckpointSpend, den, bnAccValue) || bnAccValue == 0) {
                throw new ChecksumInDbNotFoundException(
                        "calculateAccumulatedBlocksFor : failed to find checksum in database for accumulator");
            }
            accumulator.setValue(bnAccValue);
            break;
        }

        nMintsAdded += AddBlockMintsToAccumulator(den, filter, pindex, &witnessAccumulator, true, ret);

        // 10 blocks were accumulated twice when zPIV v2 was activated
        if (pindex->nHeight == 1050010 && !fDoubleCounted) {
            pindex = chainActive[1050000];
            fDoubleCounted = true;
            continue;
        }

        pindex = chainActive.Next(pindex);
    }

    // A certain amount of accumulated coins are required
    if (nMintsAdded < Params().Zerocoin_RequiredAccumulation()) {
        strError = _(strprintf("Less than %d mints added, unable to create spend",
                               Params().Zerocoin_RequiredAccumulation()).c_str());
        throw NotEnoughMintsException(strError);
    }

    LogPrintf("calculateAccumulatedBlocksFor() : nMintsAdded %d",nMintsAdded);

    return true;
}

bool calculateAccumulatedBlocksFor(
        int startHeight,
        int nHeightStop,
        int nHeightMintAdded,
        CBlockIndex *pindex,
        int &nCheckpointsAdded,
        CBigNum &bnAccValue,
        libzerocoin::Accumulator &accumulator,
        libzerocoin::Accumulator &witnessAccumulator,
        libzerocoin::PublicCoin coin,
        std::string& strError
){

    int amountOfScannedBlocks = 0;
    bool fDoubleCounted = false;
    int nMintsAdded = 0;
    while (pindex) {

        if (pindex->nHeight >= nHeightStop) {
            //If this height is within the invalid range (when fraudulent coins were being minted), then continue past this range
            if(InvalidCheckpointRange(pindex->nHeight))
                continue;

            bnAccValue = 0;
            uint256 nCheckpointSpend = chainActive[pindex->nHeight + 10]->nAccumulatorCheckpoint;
            if (!GetAccumulatorValueFromDB(nCheckpointSpend, coin.getDenomination(), bnAccValue) || bnAccValue == 0) {
                throw new ChecksumInDbNotFoundException(
                        "calculateAccumulatedBlocksFor : failed to find checksum in database for accumulator");
            }
            accumulator.setValue(bnAccValue);
            break;
        }

        // Add it
        nMintsAdded += AddBlockMintsToAccumulator(coin, nHeightMintAdded, pindex, &witnessAccumulator, true);

        // 10 blocks were accumulated twice when zPIV v2 was activated
        if (pindex->nHeight == 1050010 && !fDoubleCounted) {
            pindex = chainActive[1050000];
            fDoubleCounted = true;
            continue;
        }

        amountOfScannedBlocks++;
        pindex = chainActive.Next(pindex);
    }

    // A certain amount of accumulated coins are required
    if (nMintsAdded < Params().Zerocoin_RequiredAccumulation()) {
        strError = _(strprintf("Less than %d mints added, unable to create spend",
                               Params().Zerocoin_RequiredAccumulation()).c_str());
        throw NotEnoughMintsException(strError);
    }

    LogPrintf("calculateAccumulatedBlocksFor() : nMintsAdded %d",nMintsAdded);

    return true;
}


bool CalculateAccumulatorWitnessFor(
        const libzerocoin::ZerocoinParams* params,
        int startHeight,
        int maxCalulationRange,
        libzerocoin::CoinDenomination den,
        const CBloomFilter& filter,
        libzerocoin::Accumulator& accumulator,
        libzerocoin::AccumulatorWitness& witness,
        int& nMintsAdded,
        std::string& strError,
        std::list<CBigNum>& ret,
        int &heightStop
){
    // Lock
    if (!LockMethod()) return false;

    try {
        // Dummy coin init
        libzerocoin::PublicCoin temp(params, 0, den);
        // Dummy Acc init
        libzerocoin::Accumulator testingAcc(params, den);

        //get the checkpoint added at the next multiple of 10
        int nHeightCheckpoint = startHeight + (10 - (startHeight % 10));

        // Get the base accumulator
        // TODO: This needs to be changed to the partial witness calculation on the next version.
        CBigNum bnAccValue = 0;
        if (GetAccumulatorValue(nHeightCheckpoint, den, bnAccValue)) {
            accumulator.setValue(bnAccValue);
            witness.resetValue(accumulator, temp);
        }

        // Add the pubcoins from the blockchain up to the next checksum starting from the block
        CBlockIndex *pindex = chainActive[nHeightCheckpoint -10];
        int nChainHeight = chainActive.Height();
        int nHeightStop = nChainHeight % 10;
        nHeightStop = nChainHeight - nHeightStop - 20; // at least two checkpoints deep

        if (nHeightStop - startHeight > maxCalulationRange) {
            int stop = (startHeight + maxCalulationRange);
            int nHeightStop = stop % 10;
            nHeightStop = stop - nHeightStop - 20;
        }
        heightStop = nHeightStop;

        nMintsAdded = 0;

        // Starts on top of the witness that the node sent
        libzerocoin::Accumulator witnessAccumulator(params, den, witness.getValue());

        if(!calculateAccumulatedBlocksFor(
                startHeight,
                nHeightStop,
                pindex,
                bnAccValue,
                accumulator,
                den,
                filter,
                witnessAccumulator,
                ret,
                strError
        ))
            return error("CalculateAccumulatorWitnessFor(): Calculate accumulated coins failed");

        // reset the value
        witness.resetValue(witnessAccumulator, temp);

        // calculate how many mints of this denomination existed in the accumulator we initialized
        nMintsAdded += ComputeAccumulatedCoins(startHeight, den);
        LogPrint("zero", "%s : %d mints added to witness\n", __func__, nMintsAdded);

        return true;

    } catch (ChecksumInDbNotFoundException e) {
        return error("%s: ChecksumInDbNotFoundException: %s", __func__, e.message);
    } catch (GetPubcoinException e) {
        return error("%s: GetPubcoinException: %s", __func__, e.message);
    }
}

bool GenerateAccumulatorWitness(
        const libzerocoin::PublicCoin &coin,
        libzerocoin::Accumulator& accumulator,
        libzerocoin::AccumulatorWitness& witness,
        int& nMintsAdded,
        std::string& strError,
        CBlockIndex* pindexCheckpoint)
{
    try {
        // Lock
        LogPrint("zero", "%s: generating\n", __func__);
        if (!LockMethod()) return false;
        LogPrint("zero", "%s: after lock\n", __func__);

        int nHeightMintAdded = SearchMintHeightOf(coin.getValue());
        //get the checkpoint added at the next multiple of 10
        int nHeightCheckpoint = nHeightMintAdded + (10 - (nHeightMintAdded % 10));
        //the height to start accumulating coins to add to witness
        int nAccStartHeight = nHeightMintAdded - (nHeightMintAdded % 10);

        //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
        CBigNum bnAccValue = 0;
        if (GetAccumulatorValue(nHeightCheckpoint, coin.getDenomination(), bnAccValue)) {
            if(!bnAccValue && Params().NetworkID() == CBaseChainParams::REGTEST){
                accumulator.setInitialValue();
                witness.resetValue(accumulator, coin);
            }else {
                accumulator.setValue(bnAccValue);
                witness.resetValue(accumulator, coin);
            }
        }

        //add the pubcoins from the blockchain up to the next checksum starting from the block
        CBlockIndex *pindex = chainActive[nHeightCheckpoint - 10];
        int nChainHeight = chainActive.Height();
        int nHeightStop = nChainHeight % 10;
        nHeightStop = nChainHeight - nHeightStop - 20; // at least two checkpoints deep

        //If looking for a specific checkpoint
        if (pindexCheckpoint)
            nHeightStop = pindexCheckpoint->nHeight - 10;

        //Iterate through the chain and calculate the witness
        int nCheckpointsAdded = 0;
        nMintsAdded = 0;
        libzerocoin::Accumulator witnessAccumulator = accumulator;

        if(!calculateAccumulatedBlocksFor(
                nAccStartHeight,
                nHeightStop,
                nHeightMintAdded,
                pindex,
                nCheckpointsAdded,
                bnAccValue,
                accumulator,
                witnessAccumulator,
                coin,
                strError
        )){
            return error("GenerateAccumulatorWitness(): Calculate accumulated coins failed");
        }

        witness.resetValue(witnessAccumulator, coin);
        if (!witness.VerifyWitness(accumulator, coin))
            return error("%s: failed to verify witness", __func__);

        // A certain amount of accumulated coins are required
        if (nMintsAdded < Params().Zerocoin_RequiredAccumulation()) {
            strError = _(strprintf("Less than %d mints added, unable to create spend",
                                   Params().Zerocoin_RequiredAccumulation()).c_str());
            return error("%s : %s", __func__, strError);
        }

        // calculate how many mints of this denomination existed in the accumulator we initialized
        nMintsAdded += ComputeAccumulatedCoins(nAccStartHeight, coin.getDenomination());
        LogPrint("zero", "%s : %d mints added to witness\n", __func__, nMintsAdded);

        return true;

    // TODO: I know that could merge all of this exception but maybe it's not really good.. think if we should have a different treatment for each one
    } catch (searchMintHeightException e) {
        return error("%s: searchMintHeightException: %s", __func__, e.message);
    } catch (ChecksumInDbNotFoundException e) {
        return error("%s: ChecksumInDbNotFoundException: %s", __func__, e.message);
    } catch (GetPubcoinException e) {
        return error("%s: GetPubcoinException: %s", __func__, e.message);
    }
}



std::map<libzerocoin::CoinDenomination, int> GetMintMaturityHeight()
{
    std::map<libzerocoin::CoinDenomination, std::pair<int, int > > mapDenomMaturity;
    for (auto denom : libzerocoin::zerocoinDenomList)
        mapDenomMaturity.insert(std::make_pair(denom, std::make_pair(0, 0)));

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
    std::map<libzerocoin::CoinDenomination, int> mapRet;
    for (auto denom : libzerocoin::zerocoinDenomList)
        mapRet.insert(std::make_pair(denom, mapDenomMaturity.at(denom).second));

    return mapRet;
}
