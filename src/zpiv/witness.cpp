#include <chainparams.h>
#include <tinyformat.h>
#include "witness.h"

void CoinWitnessData::SetNull()
{
    coin = nullptr;
    pAccumulator = nullptr;
    pWitness = nullptr;
    nMintsAdded = 0;
    nHeightMintAdded = 0;
    nHeightCheckpoint = 0;
    nHeightAccStart = 0;
    nHeightAccEnd = 0;
}

CoinWitnessData::CoinWitnessData()
{
    SetNull();
}

std::string CoinWitnessData::ToString()
{
    return strprintf("Mints Added: %d\n"
            "Height Mint added: %d\n"
            "Height Checkpoint: %d\n"
            "Height Acc Start: %d\n"
            "Height Acc End: %d\n"
            "Amount: %s\n"
            "Demon: %d\n", nMintsAdded, nHeightMintAdded, nHeightCheckpoint, nHeightAccStart, nHeightAccEnd, coin->getValue().GetHex(), coin->getDenomination());
}

CoinWitnessData::CoinWitnessData(CZerocoinMint& mint)
{
    SetNull();
    denom = mint.GetDenomination();
    isV1 = libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber()) < libzerocoin::PrivateCoin::PUBKEY_VERSION;
    libzerocoin::ZerocoinParams* paramsCoin = Params().Zerocoin_Params(isV1);
    coin = std::unique_ptr<libzerocoin::PublicCoin>(new libzerocoin::PublicCoin(paramsCoin, mint.GetValue(), denom));
    libzerocoin::Accumulator accumulator1(Params().Zerocoin_Params(false), denom);
    pWitness = std::unique_ptr<libzerocoin::AccumulatorWitness>(new libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(false), accumulator1, *coin));
    nHeightAccStart = mint.GetHeight();
}

CoinWitnessData::CoinWitnessData(CoinWitnessCacheData& data)
{
    SetNull();
    denom = data.denom;
    isV1 = data.isV1;
    libzerocoin::ZerocoinParams* paramsCoin = Params().Zerocoin_Params(isV1);
    coin = std::unique_ptr<libzerocoin::PublicCoin>(new libzerocoin::PublicCoin(paramsCoin, data.coinAmount, data.coinDenom));
    pAccumulator = std::unique_ptr<libzerocoin::Accumulator>(new libzerocoin::Accumulator(Params().Zerocoin_Params(false), denom, data.accumulatorAmount));
    pWitness = std::unique_ptr<libzerocoin::AccumulatorWitness>(new libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(false), *pAccumulator, *coin));
    nMintsAdded = data.nMintsAdded;
    nHeightMintAdded = data.nHeightMintAdded;
    nHeightCheckpoint = data.nHeightCheckpoint;
    nHeightAccStart = data.nHeightAccStart;
    nHeightAccEnd = data.nHeightAccEnd;
    txid = data.txid;
}

void CoinWitnessData::SetHeightMintAdded(int nHeight)
{
    nHeightMintAdded = nHeight;
    nHeightCheckpoint = nHeight + (10 - (nHeight % 10));
    nHeightAccStart = nHeight - (nHeight % 10);
}



void CoinWitnessCacheData::SetNull()
{
    nMintsAdded = 0;
    nHeightMintAdded = 0;
    nHeightCheckpoint = 0;
    nHeightAccStart = 0;
    nHeightAccEnd = 0;
    coinAmount = CBigNum(0);
    coinDenom = libzerocoin::CoinDenomination::ZQ_ERROR;
    accumulatorAmount = CBigNum(0);
    accumulatorDenom = libzerocoin::CoinDenomination::ZQ_ERROR;

}

CoinWitnessCacheData::CoinWitnessCacheData()
{
    SetNull();
}

CoinWitnessCacheData::CoinWitnessCacheData(CoinWitnessData* coinWitnessData)
{
    SetNull();
    denom = coinWitnessData->denom;
    isV1 = coinWitnessData->isV1;
    txid = coinWitnessData->txid;
    nMintsAdded = coinWitnessData->nMintsAdded;
    nHeightMintAdded = coinWitnessData->nHeightMintAdded;
    nHeightCheckpoint = coinWitnessData->nHeightCheckpoint;
    nHeightAccStart = coinWitnessData->nHeightAccStart;
    nHeightAccEnd = coinWitnessData->nHeightAccEnd;
    coinAmount = coinWitnessData->coin->getValue();
    coinDenom = coinWitnessData->coin->getDenomination();
    accumulatorAmount = coinWitnessData->pAccumulator->getValue();
    accumulatorDenom = coinWitnessData->pAccumulator->getDenomination();
}

