// Copyright (c) 2017-2018 The PIVX developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "chain.h"
#include "primitives/deterministicmint.h"
#include "main.h"
#include "stakeinput.h"
#include "wallet.h"

CZSynxStake::CZSynxStake(const libzerocoin::CoinSpend& spend)
{
    this->nChecksum = spend.getAccumulatorChecksum();
    this->denom = spend.getDenomination();
    uint256 nSerial = spend.getCoinSerialNumber().getuint256();
    this->hashSerial = Hash(nSerial.begin(), nSerial.end());
    this->pindexFrom = nullptr;
    fMint = false;
}

int CZSynxStake::GetChecksumHeightFromMint()
{
    int nHeightChecksum = chainActive.Height() - Params().Zerocoin_RequiredStakeDepth();

    //Need to return the first occurance of this checksum in order for the validation process to identify a specific
    //block height
    uint32_t nChecksum = 0;
    nChecksum = ParseChecksum(chainActive[nHeightChecksum]->nAccumulatorCheckpoint, denom);
    return GetChecksumHeight(nChecksum, denom);
}

int CZSynxStake::GetChecksumHeightFromSpend()
{
    return GetChecksumHeight(nChecksum, denom);
}

uint32_t CZSynxStake::GetChecksum()
{
    return nChecksum;
}

// The zSYNX block index is the first appearance of the accumulator checksum that was used in the spend
// note that this also means when staking that this checksum should be from a block that is beyond 60 minutes old and
// 100 blocks deep.
CBlockIndex* CZSynxStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;

    int nHeightChecksum = 0;

    if (fMint)
        nHeightChecksum = GetChecksumHeightFromMint();
    else
        nHeightChecksum = GetChecksumHeightFromSpend();

    if (nHeightChecksum < Params().Zerocoin_StartHeight() || nHeightChecksum > chainActive.Height()) {
        pindexFrom = nullptr;
    } else {
        //note that this will be a nullptr if the height DNE
        pindexFrom = chainActive[nHeightChecksum];
    }

    return pindexFrom;
}

CAmount CZSynxStake::GetValue()
{
    return denom * COIN;
}

//Use the first accumulator checkpoint that occurs 60 minutes after the block being staked from
bool CZSynxStake::GetModifier(uint64_t& nStakeModifier)
{
    CBlockIndex* pindex = GetIndexFrom();
    if (!pindex)
        return false;

    int64_t nTimeBlockFrom = pindex->GetBlockTime();
    while (true) {
        if (pindex->GetBlockTime() - nTimeBlockFrom > 60*60) {
            nStakeModifier = pindex->nAccumulatorCheckpoint.Get64();
            return true;
        }

        if (pindex->nHeight + 1 <= chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            return false;
    }
}

CDataStream CZSynxStake::GetUniqueness()
{
    //The unique identifier for a zSYNX is a hash of the serial
    CDataStream ss(SER_GETHASH, 0);
    ss << hashSerial;
    return ss;
}

bool CZSynxStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    CBlockIndex* pindexCheckpoint = GetIndexFrom();
    if (!pindexCheckpoint)
        return error("%s: failed to find checkpoint block index", __func__);

    CZerocoinMint mint;
    if (!pwallet->GetMintFromStakeHash(hashSerial, mint))
        return error("%s: failed to fetch mint associated with serial hash %s", __func__, hashSerial.GetHex());

    if (libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber()) < 2)
        return error("%s: serial extract is less than v2", __func__);

    int nSecurityLevel = 100;
    CZerocoinSpendReceipt receipt;
    if (!pwallet->MintToTxIn(mint, nSecurityLevel, hashTxOut, txIn, receipt, libzerocoin::SpendType::STAKE, GetIndexFrom()))
        return error("%s\n", receipt.GetStatusMessage());

    return true;
}

bool CZSynxStake::CreateTxOuts(CWallet* pwallet, vector<CTxOut>& vout, CAmount nTotal)
{
    //Create an output returning the zSYNX that was staked
    CTxOut outReward;
    libzerocoin::CoinDenomination denomStaked = libzerocoin::AmountToZerocoinDenomination(this->GetValue());
    CDeterministicMint dMint;
    if (!pwallet->CreateZSYNXOutPut(denomStaked, outReward, dMint))
        return error("%s: failed to create zSYNX output", __func__);
    vout.emplace_back(outReward);

    //Add new staked denom to our wallet
    if (!pwallet->DatabaseMint(dMint))
        return error("%s: failed to database the staked zSYNX", __func__);

    for (unsigned int i = 0; i < 3; i++) {
        CTxOut out;
        CDeterministicMint dMintReward;
        if (!pwallet->CreateZSYNXOutPut(libzerocoin::CoinDenomination::ZQ_ONE, out, dMintReward))
            return error("%s: failed to create zSYNX output", __func__);
        vout.emplace_back(out);

        if (!pwallet->DatabaseMint(dMintReward))
            return error("%s: failed to database mint reward", __func__);
    }

    return true;
}

bool CZSynxStake::GetTxFrom(CTransaction& tx)
{
    return false;
}

bool CZSynxStake::MarkSpent(CWallet *pwallet, const uint256& txid)
{
    CzSYNXTracker* zsynxTracker = pwallet->zsynxTracker.get();
    CMintMeta meta;
    if (!zsynxTracker->GetMetaFromStakeHash(hashSerial, meta))
        return error("%s: tracker does not have serialhash", __func__);

    zsynxTracker->SetPubcoinUsed(meta.hashPubcoin, txid);
    return true;
}

//!SYNX Stake
bool CSynxStake::SetInput(CTransaction txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CSynxStake::GetTxFrom(CTransaction& tx)
{
    tx = txFrom;
    return true;
}

bool CSynxStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom.GetHash(), nPosition);
    return true;
}

CAmount CSynxStake::GetValue()
{
    return txFrom.vout[nPosition].nValue;
}

bool CSynxStake::CreateTxOuts(CWallet* pwallet, vector<CTxOut>& vout, CAmount nTotal)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom.vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
        LogPrintf("CreateCoinStake : failed to parse kernel\n");
        return false;
    }

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
        return false; // only support pay to public key and pay to address

    CScript scriptPubKey;
    if (whichType == TX_PUBKEYHASH) // pay to address type
    {
        //convert to pay to public key type
        CKey key;
        CKeyID keyID = CKeyID(uint160(vSolutions[0]));
        if (!pwallet->GetKey(keyID, key))
            return false;

        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;
    } else
        scriptPubKey = scriptPubKeyKernel;

    vout.emplace_back(CTxOut(0, scriptPubKey));

    // Calculate if we need to split the output
    if (nTotal / 2 > (CAmount)(pwallet->nStakeSplitThreshold * COIN))
        vout.emplace_back(CTxOut(0, scriptPubKey));

    return true;
}

bool CSynxStake::GetModifier(uint64_t& nStakeModifier)
{
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
    GetIndexFrom();
    if (!pindexFrom)
        return error("%s: failed to get index from", __func__);

    if (!GetKernelStakeModifier(pindexFrom->GetBlockHash(), nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false))
        return error("CheckStakeKernelHash(): failed to get kernel stake modifier \n");

    return true;
}

CDataStream CSynxStake::GetUniqueness()
{
    //The unique identifier for a SYNX stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << nPosition << txFrom.GetHash();
    return ss;
}

//The block that the UTXO was added to the chain
CBlockIndex* CSynxStake::GetIndexFrom()
{
    uint256 hashBlock = 0;
    CTransaction tx;
    if (GetTransaction(txFrom.GetHash(), tx, hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom.GetHash().GetHex());
    }

    return pindexFrom;
}