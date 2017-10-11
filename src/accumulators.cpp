// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include "init.h"
#include "spork.h"

using namespace libzerocoin;

void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    for (auto& denom : zerocoinDenomList) {
        unique_ptr<Accumulator> uptr(new Accumulator(Params().Zerocoin_Params(), denom));
        mapAccumulators.insert(make_pair(denom, std::move(uptr)));
    }
}

Accumulator CAccumulators::Get(CoinDenomination denomination)
{
    return Accumulator(Params().Zerocoin_Params(), denomination, mapAccumulators.at(denomination)->getValue());
}

bool CAccumulators::AddPubCoinToAccumulator(const PublicCoin& publicCoin)
{
    CoinDenomination denomination = publicCoin.getDenomination();
    if(mapAccumulators.find(denomination) == mapAccumulators.end()) {
        LogPrintf("%s: failed to find accumulator for %d\n", __func__, denomination);
        return false;
    }

    mapAccumulators.at(denomination)->accumulate(publicCoin);
    LogPrint("zero", "%s: Accumulated %d\n", __func__, denomination);
    return true;
}

uint32_t CAccumulators::GetChecksum(const CBigNum &bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return hash.Get32();
}

uint32_t CAccumulators::GetChecksum(const Accumulator &accumulator)
{
    return GetChecksum(accumulator.getValue());
}

void CAccumulators::DatabaseChecksums(const uint256& nCheckpoint)
{
    uint256 nCheckpointCalculated = 0;
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.at(denom)->getValue();
        uint32_t nCheckSum = GetChecksum(bnValue);
        AddAccumulatorChecksum(nCheckSum, bnValue);
        nCheckpointCalculated = nCheckpointCalculated << 32 | nCheckSum;
    }
}

void CAccumulators::AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly)
{
    if(!fMemoryOnly)
        zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
    mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));

    LogPrint("zero", "%s checksum %d val %s\n", __func__, nChecksum, bnValue.GetHex());
    LogPrint("zero", "%s map val %s\n", __func__, mapAccumulatorValues[nChecksum].GetHex());
}

bool CAccumulators::LoadAccumulatorValuesFromDB(const uint256 nCheckpoint)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);

        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue)) {
            LogPrint("zero","%s : Missing databased value for checksum %d\n", __func__, nChecksum);
            if (!count(listAccCheckpointsNoDB.begin(), listAccCheckpointsNoDB.end(), nCheckpoint))
                listAccCheckpointsNoDB.push_back(nCheckpoint);
            return false;
        }
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
    return true;
}

bool CAccumulators::EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksumErase = ParseChecksum(nCheckpointErase, denomination);
        uint32_t nChecksumPrevious = ParseChecksum(nCheckpointPrevious, denomination);

        //if the previous checksum is the same, then it should remain in the database and map
        if(nChecksumErase == nChecksumPrevious)
            continue;

        //erase from both memory and database
        mapAccumulatorValues.erase(nChecksumErase);
        if(!zerocoinDB->EraseAccumulatorValue(nChecksumErase))
            return false;
    }

    return true;
}

bool CAccumulators::EraseCoinMint(const CBigNum& bnPubCoin)
{
    return zerocoinDB->EraseCoinMint(bnPubCoin);
}

bool CAccumulators::EraseCoinSpend(const CBigNum& bnSerial)
{
    mapSerials.erase(bnSerial);
    return zerocoinDB->EraseCoinSpend(bnSerial);
}

uint32_t ParseChecksum(uint256 nChecksum, CoinDenomination denomination)
{
    //shift to the beginning bit of this denomination and trim any remaining bits by returning 32 bits only
    int pos = distance(zerocoinDenomList.begin(), find(zerocoinDenomList.begin(), zerocoinDenomList.end(), denomination));
    nChecksum = nChecksum >> (32*((zerocoinDenomList.size() - 1) - pos));
    return nChecksum.Get32();
}

CBigNum CAccumulators::GetAccumulatorValueFromCheckpoint(const uint256& nCheckpoint, CoinDenomination denomination)
{
    uint32_t nDenominationChecksum = ParseChecksum(nCheckpoint, denomination);
    LogPrint("zero", "%s checkpoint:%d\n", __func__, nCheckpoint.GetHex());
    LogPrint("zero", "%s checksum:%d\n", __func__, nDenominationChecksum);

    return GetAccumulatorValueFromChecksum(nDenominationChecksum);
}

CBigNum CAccumulators::GetAccumulatorValueFromChecksum(const uint32_t& nChecksum)
{
    if(!mapAccumulatorValues.count(nChecksum))
        return CBigNum(0);

    return mapAccumulatorValues[nChecksum];
}

//set all of the accumulators held by mapAccumulators to a certain checkpoint
bool CAccumulators::ResetToCheckpoint(const uint256& nCheckpoint)
{
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = GetAccumulatorValueFromCheckpoint(nCheckpoint, denom);
        if (bnValue == 0) {
            //if the value is zero, then this is an unused accumulator and must be reinitialized
            unique_ptr<Accumulator> uptr(new Accumulator(Params().Zerocoin_Params(), denom));
            mapAccumulators.at(denom) = std::move(uptr);
            continue;
        }

        mapAccumulators.at(denom)->setValue(bnValue);
    }

    return true;
}

//Get checkpoint value from the current state of our accumulator map
uint256 CAccumulators::GetCheckpoint()
{
    uint256 nCheckpoint;
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.at(denom)->getValue();
        uint32_t nCheckSum = GetChecksum(bnValue);
        AddAccumulatorChecksum(nCheckSum, bnValue);
        nCheckpoint = nCheckpoint << 32 | nCheckSum;

        LogPrint("zero", "%s: Acc value:%s\n", __func__, bnValue.GetHex());
        LogPrint("zero", "%s: checksum value:%d\n", __func__, nCheckSum);
        LogPrint("zero", "%s: checkpoint %s\n", __func__, nCheckpoint.GetHex());
    }

    return nCheckpoint;
}

//Get checkpoint value for a specific block height
bool CAccumulators::GetCheckpoint(int nHeight, uint256& nCheckpoint)
{
    if (nHeight <= chainActive.Height() && chainActive[nHeight]->GetBlockHeader().nVersion < Params().Zerocoin_HeaderVersion()) {
        nCheckpoint = 0;
        return true;
    }

    //the checkpoint is updated every ten blocks, return current active checkpoint if not update block
    if (nHeight % 10 != 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
        return true;
    }

    //set the accumulators to last checkpoint value
    if(!ResetToCheckpoint(chainActive[nHeight - 1]->nAccumulatorCheckpoint)) {
        LogPrint("zero","%s: failed to reset to previous checkpoint\n", __func__);
        return false;
    }

    //Accumulate all coins over the last ten blocks that havent been accumulated (height - 20 through height - 11)
    int nTotalMintsFound = 0;
    CBlockIndex *pindex = chainActive[nHeight - 20];
    while (pindex->nHeight < nHeight - 10) {
        //make sure this block is eligible for accumulation
        if (pindex->GetBlockHeader().nVersion < Params().Zerocoin_HeaderVersion()) {
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex)) {
            LogPrint("zero","%s: failed to read block from disk\n", __func__);
            return false;
        }
        std::list<PublicCoin> listPubcoins;
        if(!BlockToPubcoinList(block, listPubcoins)) {
            LogPrint("zero","%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
            return false;
        }

        nTotalMintsFound += listPubcoins.size();
        LogPrint("zero", "%s found %d mints\n", __func__, listPubcoins.size());

        //add the pubcoins to accumulator
        for(const PublicCoin pubcoin : listPubcoins) {
            if(!AddPubCoinToAccumulator(pubcoin)) {
                LogPrint("zero","%s: failed to add pubcoin to accumulator at height %n\n", __func__, pindex->nHeight);
                return false;
            }
        }
        pindex = chainActive[pindex->nHeight + 1];
    }

    // if there were no new mints found, the accumulator checkpoint will be the same as the last checkpoint
    if (nTotalMintsFound == 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;

        // make sure that these values are databased because reorgs may have deleted the checksums from DB
        DatabaseChecksums(nCheckpoint);
    }
    else
        nCheckpoint = GetCheckpoint();

    LogPrint("zero", "%s checkpoint=%s\n", __func__, nCheckpoint.GetHex());
    return true;
}

bool CAccumulators::IntializeWitnessAndAccumulator(const PublicCoin &coin, Accumulator& accumulator, AccumulatorWitness& witness, int nSecurityLevel, int& nMintsAdded, string& strError)
{
    uint256 txid;
    if (!zerocoinDB->ReadCoinMint(coin.getValue(), txid)) {
        LogPrint("zero","%s failed to read mint from db\n", __func__);
        return false;
    }

    CTransaction txMinted;
    uint256 hashBlock;
    if (!GetTransaction(txid, txMinted, hashBlock)) {
        LogPrint("zero","%s failed to read tx\n", __func__);
        return false;
    }

    int nHeightMintAdded= mapBlockIndex[hashBlock]->nHeight;
    uint256 nChecksumBeforeMint = 0, nChecksumContainingMint = 0;
    CBlockIndex* pindex = chainActive[nHeightMintAdded];
    int nChanges = 0;

    //find the checksum when this was added to the accumulator officially, which will be two checksum changes later
    //reminder that checksums are generated when the block height is a multiple of 10
    while (pindex->nHeight < chainActive.Tip()->nHeight - 1) {
        if (pindex->nHeight == nHeightMintAdded) {
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //check if the next checksum was generated
        if (pindex->nHeight % 10 == 0) {
            nChecksumContainingMint = pindex->nAccumulatorCheckpoint;
            nChanges++;

            if (nChanges == 1)
                nChecksumBeforeMint = pindex->nAccumulatorCheckpoint;
            else if (nChanges == 2)
                break;
        }
        pindex = chainActive[pindex->nHeight + 1];
    }

    //the height to start accumulating coins to add to witness
    int nAccStartHeight = nHeightMintAdded - (nHeightMintAdded % 10);

    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    CBigNum bnAccValue = GetAccumulatorValueFromCheckpoint(nChecksumBeforeMint, coin.getDenomination());
    if (bnAccValue != 0) {
        accumulator.setValue(bnAccValue);
        witness.resetValue(accumulator, coin);
    }

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

    //add the pubcoins (zerocoinmints that have been published to the chain) up to the next checksum starting from the block
    pindex = chainActive[nAccStartHeight];
    int nChainHeight = chainActive.Height();
    int nHeightStop = nChainHeight % 10;
    nHeightStop = nChainHeight - nHeightStop - 20; // at least two checkpoints deep
    int nCheckpointsAdded = 0;
    nMintsAdded = 0;
    while (pindex->nHeight < nHeightStop + 1) {
        if (pindex->nHeight != nAccStartHeight && pindex->pprev->nAccumulatorCheckpoint != pindex->nAccumulatorCheckpoint)
            ++nCheckpointsAdded;

        //if a new checkpoint was generated on this block, and we have added the specified amount of checkpointed accumulators,
        //then initialize the accumulator at this point and break
        if (pindex->nHeight == nHeightStop || (nSecurityLevel != 100 && nCheckpointsAdded >= nSecurityLevel)) {
            CBigNum bnAccValue = GetAccumulatorValueFromCheckpoint(chainActive[pindex->nHeight + 10]->nAccumulatorCheckpoint, coin.getDenomination());
            accumulator.setValue(bnAccValue);
            break;
        }

        // if this block contains mints of the denomination that is being spent, then add them to the witness
        if (pindex->MintedDenomination(coin.getDenomination())) {
            //grab mints from this block
            CBlock block;
            if(!ReadBlockFromDisk(block, pindex)) {
                LogPrint("zero","%s: failed to read block from disk while adding pubcoins to witness\n", __func__);
                return false;
            }

            std::vector<CBigNum> vValues;
            if(!BlockToMintValueVector(block, coin.getDenomination(), vValues)) {
                LogPrint("zero","%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
                return false;
            }

            //add the mints to the witness
            for (const CBigNum bnValue : vValues) {
                if(pindex->nHeight == nHeightMintAdded && bnValue == coin.getValue())
                    continue;

                witness.addRawValue(bnValue);
                ++nMintsAdded;
            }
        }

        pindex = chainActive[pindex->nHeight + 1];
    }

    if (nMintsAdded < 3) {
        strError = _("Less than 3 mints added, unable to create spend");
        LogPrintf("%s : %s\n", __func__, strError);
        return false;
    }

    // calculate how many mints of this denomination existed in the accumulator we initialized
    int nZerocoinStartHeight = GetZerocoinStartHeight();
    pindex = chainActive[nZerocoinStartHeight];
    while (pindex->nHeight < nAccStartHeight) {
        nMintsAdded += count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), coin.getDenomination());
        pindex = chainActive[pindex->nHeight + 1];
    }
    
    LogPrint("zero","%s : %d mints added to witness\n", __func__, nMintsAdded);
    return true;
}
