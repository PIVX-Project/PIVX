// Copyright (c) 2017-2017 The Bitcoin Core developers
// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zerocoin_verify.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "guiinterface.h"        // for ui_interface
#include "init.h"                // for ShutdownRequested()
#include "main.h"
#include "script/interpreter.h"
#include "spork.h"               // for sporkManager
#include "txdb.h"                // for class CZerocoinDB
#include "utilmoneystr.h"        // for FormatMoney

bool CheckZerocoinMint(const uint256& txHash, const CTxOut& txout, CValidationState& state, bool fCheckOnly)
{
    libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(false));
    if(!TxOutToPublicCoin(txout, pubCoin, state))
        return state.DoS(100, error("CheckZerocoinMint(): TxOutToPublicCoin() failed"));

    if (!pubCoin.validate())
        return state.DoS(100, error("CheckZerocoinMint() : PubCoin does not validate"));

    return true;
}

bool ContextualCheckZerocoinMint(const CTransaction& tx, const libzerocoin::PublicCoin& coin, const CBlockIndex* pindex)
{
    if (pindex->nHeight >= Params().Zerocoin_Block_Public_Spend_Enabled()) {
        // Zerocoin MINTs have been disabled
        return error("%s: Mints disabled at height %d - unable to add pubcoin %s", __func__,
                pindex->nHeight, coin.getValue().GetHex().substr(0, 10));
    }
    if (pindex->nHeight >= Params().Zerocoin_Block_V2_Start() && Params().NetworkID() != CBaseChainParams::TESTNET) {
        //See if this coin has already been added to the blockchain
        uint256 txid;
        int nHeight;
        if (zerocoinDB->ReadCoinMint(coin.getValue(), txid) && IsTransactionInChain(txid, nHeight))
            return error("%s: pubcoin %s was already accumulated in tx %s", __func__,
                         coin.getValue().GetHex().substr(0, 10),
                         txid.GetHex());
    }

    return true;
}

bool ContextualCheckZerocoinSpend(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashBlock)
{
    if(!ContextualCheckZerocoinSpendNoSerialCheck(tx, spend, pindex, hashBlock)){
        return false;
    }

    //Reject serial's that are already in the blockchain
    int nHeightTx = 0;
    if (IsSerialInBlockchain(spend->getCoinSerialNumber(), nHeightTx))
        return error("%s : zPIV spend with serial %s is already in block %d\n", __func__,
                     spend->getCoinSerialNumber().GetHex(), nHeightTx);

    return true;
}

bool ContextualCheckZerocoinSpendNoSerialCheck(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashBlock)
{
    //Check to see if the zPIV is properly signed
    if (pindex->nHeight >= Params().Zerocoin_Block_V2_Start()) {
        try {
            if (!spend->HasValidSignature())
                return error("%s: V2 zPIV spend does not have a valid signature\n", __func__);
        } catch (const libzerocoin::InvalidSerialException& e) {
            // Check if we are in the range of the attack
            if(!isBlockBetweenFakeSerialAttackRange(pindex->nHeight))
                return error("%s: Invalid serial detected, txid %s, in block %d\n", __func__, tx.GetHash().GetHex(), pindex->nHeight);
            else
                LogPrintf("%s: Invalid serial detected within range in block %d\n", __func__, pindex->nHeight);
        }

        libzerocoin::SpendType expectedType = libzerocoin::SpendType::SPEND;
        if (tx.IsCoinStake())
            expectedType = libzerocoin::SpendType::STAKE;
        if (spend->getSpendType() != expectedType) {
            return error("%s: trying to spend zPIV without the correct spend type. txid=%s\n", __func__,
                         tx.GetHash().GetHex());
        }
    }

    bool fUseV1Params = spend->getCoinVersion() < libzerocoin::PrivateCoin::PUBKEY_VERSION;

    //Reject serial's that are not in the acceptable value range
    if (!spend->HasValidSerial(Params().Zerocoin_Params(fUseV1Params)))  {
        // Up until this block our chain was not checking serials correctly..
        if (!isBlockBetweenFakeSerialAttackRange(pindex->nHeight))
            return error("%s : zPIV spend with serial %s from tx %s is not in valid range\n", __func__,
                     spend->getCoinSerialNumber().GetHex(), tx.GetHash().GetHex());
        else
            LogPrintf("%s:: HasValidSerial :: Invalid serial detected within range in block %d\n", __func__, pindex->nHeight);
    }


    return true;
}

bool CheckZerocoinSpend(const CTransaction& tx, bool fVerifySignature, CValidationState& state, bool fFakeSerialAttack)
{
    //max needed non-mint outputs should be 2 - one for redemption address and a possible 2nd for change
    if (tx.vout.size() > 2) {
        int outs = 0;
        for (const CTxOut& out : tx.vout) {
            if (out.IsZerocoinMint())
                continue;
            outs++;
        }
        if (outs > 2 && !tx.IsCoinStake())
            return state.DoS(100, error("CheckZerocoinSpend(): over two non-mint outputs in a zerocoinspend transaction"));
    }

    //compute the txout hash that is used for the zerocoinspend signatures
    CMutableTransaction txTemp;
    for (const CTxOut& out : tx.vout) {
        txTemp.vout.push_back(out);
    }
    uint256 hashTxOut = txTemp.GetHash();

    bool fValidated = false;
    std::set<CBigNum> serials;
    CAmount nTotalRedeemed = 0;
    for (const CTxIn& txin : tx.vin) {

        //only check txin that is a zcspend
        bool isPublicSpend = txin.IsZerocoinPublicSpend();
        if (!txin.IsZerocoinSpend() && !isPublicSpend)
            continue;

        libzerocoin::CoinSpend newSpend;
        CTxOut prevOut;
        if (isPublicSpend) {
            if(!GetOutput(txin.prevout.hash, txin.prevout.n, state, prevOut)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend prev output not found, prevTx %s, index %d", txin.prevout.hash.GetHex(), txin.prevout.n));
            }
            libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
            PublicCoinSpend publicSpend(params);
            if (!ZPIVModule::parseCoinSpend(txin, tx, prevOut, publicSpend)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend parse failed"));
            }
            newSpend = publicSpend;
        } else {
            newSpend = TxInToZerocoinSpend(txin);
        }

        //check that the denomination is valid
        if (newSpend.getDenomination() == libzerocoin::ZQ_ERROR)
            return state.DoS(100, error("Zerocoinspend does not have the correct denomination"));

        //check that denomination is what it claims to be in nSequence
        if (newSpend.getDenomination() != txin.nSequence)
            return state.DoS(100, error("Zerocoinspend nSequence denomination does not match CoinSpend"));

        //make sure the txout has not changed
        if (newSpend.getTxOutHash() != hashTxOut)
            return state.DoS(100, error("Zerocoinspend does not use the same txout that was used in the SoK"));

        if (isPublicSpend) {
            libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
            PublicCoinSpend ret(params);
            if (!ZPIVModule::validateInput(txin, prevOut, tx, ret)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend did not verify"));
            }
        } else
            // Skip signature verification during initial block download
            if (fVerifySignature) {
                //see if we have record of the accumulator used in the spend tx
                CBigNum bnAccumulatorValue = 0;
                if (!zerocoinDB->ReadAccumulatorValue(newSpend.getAccumulatorChecksum(), bnAccumulatorValue)) {
                    uint32_t nChecksum = newSpend.getAccumulatorChecksum();
                    return state.DoS(100, error("%s: Zerocoinspend could not find accumulator associated with checksum %s", __func__, HexStr(BEGIN(nChecksum), END(nChecksum))));
                }

                libzerocoin::Accumulator accumulator(Params().Zerocoin_Params(chainActive.Height() < Params().Zerocoin_Block_V2_Start()),
                                        newSpend.getDenomination(), bnAccumulatorValue);

                //Check that the coin has been accumulated
                if(!newSpend.Verify(accumulator, !fFakeSerialAttack))
                        return state.DoS(100, error("CheckZerocoinSpend(): zerocoin spend did not verify"));
            }

        if (serials.count(newSpend.getCoinSerialNumber()))
            return state.DoS(100, error("Zerocoinspend serial is used twice in the same tx"));
        serials.insert(newSpend.getCoinSerialNumber());

        //make sure that there is no over redemption of coins
        nTotalRedeemed += libzerocoin::ZerocoinDenominationToAmount(newSpend.getDenomination());
        fValidated = true;
    }

    if (!tx.IsCoinStake() && nTotalRedeemed < tx.GetValueOut()) {
        LogPrintf("redeemed = %s , spend = %s \n", FormatMoney(nTotalRedeemed), FormatMoney(tx.GetValueOut()));
        return state.DoS(100, error("Transaction spend more than was redeemed in zerocoins"));
    }

    return fValidated;
}

bool isBlockBetweenFakeSerialAttackRange(int nHeight)
{
    if (Params().NetworkID() != CBaseChainParams::MAIN)
        return false;

    return nHeight <= Params().Zerocoin_Block_EndFakeSerial();
}

bool CheckPublicCoinSpendEnforced(int blockHeight, bool isPublicSpend) {
    if (blockHeight >= Params().Zerocoin_Block_Public_Spend_Enabled()) {
        // reject old coin spend
        if (!isPublicSpend) {
            return error("%s: failed to add block with older zc spend version", __func__);
        }

    } else {
        if (isPublicSpend) {
            return error("%s: failed to add block, public spend enforcement not activated", __func__);
        }
    }
    return true;
}

int CurrentPublicCoinSpendVersion() {
    return sporkManager.IsSporkActive(SPORK_18_ZEROCOIN_PUBLICSPEND_V4) ? 4 : 3;
}

bool CheckPublicCoinSpendVersion(int version) {
    return version == CurrentPublicCoinSpendVersion();
}

void AddWrappedSerialsInflation()
{
    CBlockIndex* pindex = chainActive[Params().Zerocoin_Block_EndFakeSerial()];
    if (pindex->nHeight > chainActive.Height())
        return;

    uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_Block_EndFakeSerial()) * 100 / (chainActive.Height() - Params().Zerocoin_Block_EndFakeSerial()))));
            uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), percent);
        }

        // Add inflated denominations to block index mapSupply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
        }
        // Update current block index to disk
        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));
        // next block
        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

void RecalculateZPIVMinted()
{
    CBlockIndex *pindex = chainActive[Params().Zerocoin_StartHeight()];
    uiInterface.ShowProgress(_("Recalculating minted ZPIV..."), 0);
    while (true) {
        // Log Message and feedback message every 1000 blocks
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_StartHeight()) * 100 / (chainActive.Height() - Params().Zerocoin_StartHeight()))));
            uiInterface.ShowProgress(_("Recalculating minted ZPIV..."), percent);
        }

        //overwrite possibly wrong vMintsInBlock data
        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        std::list<CZerocoinMint> listMints;
        BlockToZerocoinMintList(block, listMints, true);

        std::vector<libzerocoin::CoinDenomination> vDenomsBefore = pindex->vMintDenominationsInBlock;
        pindex->vMintDenominationsInBlock.clear();
        for (auto mint : listMints)
            pindex->vMintDenominationsInBlock.emplace_back(mint.GetDenomination());

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

void RecalculateZPIVSpent()
{
    CBlockIndex* pindex = chainActive[Params().Zerocoin_StartHeight()];
    uiInterface.ShowProgress(_("Recalculating spent ZPIV..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_StartHeight()) * 100 / (chainActive.Height() - Params().Zerocoin_StartHeight()))));
            uiInterface.ShowProgress(_("Recalculating spent ZPIV..."), percent);
        }

        //Rewrite zPIV supply
        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        std::list<libzerocoin::CoinDenomination> listDenomsSpent = ZerocoinSpendListFromBlock(block, true);

        //Reset the supply to previous block
        pindex->mapZerocoinSupply = pindex->pprev->mapZerocoinSupply;

        //Add mints to zPIV supply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            long nDenomAdded = count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), denom);
            pindex->mapZerocoinSupply.at(denom) += nDenomAdded;
        }

        //Remove spends from zPIV supply
        for (auto denom : listDenomsSpent)
            pindex->mapZerocoinSupply.at(denom)--;

        // Add inflation from Wrapped Serials if block is Zerocoin_Block_EndFakeSerial()
        if (pindex->nHeight == Params().Zerocoin_Block_EndFakeSerial() + 1)
            for (auto denom : libzerocoin::zerocoinDenomList) {
                pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
            }

        //Rewrite money supply
        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

bool RecalculatePIVSupply(int nHeightStart)
{
    if (nHeightStart > chainActive.Height())
        return false;

    CBlockIndex* pindex = chainActive[nHeightStart];
    CAmount nSupplyPrev = pindex->pprev->nMoneySupply;
    if (nHeightStart == Params().Zerocoin_StartHeight())
        nSupplyPrev = CAmount(5449796547496199);

    uiInterface.ShowProgress(_("Recalculating PIV supply..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)((pindex->nHeight - nHeightStart) * 100) / (chainActive.Height() - nHeightStart))));
            uiInterface.ShowProgress(_("Recalculating PIV supply..."), percent);
        }

        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        CAmount nValueIn = 0;
        CAmount nValueOut = 0;
        for (const CTransaction& tx : block.vtx) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                if (tx.IsCoinBase())
                    break;

                if (tx.vin[i].IsZerocoinSpend()) {
                    nValueIn += tx.vin[i].nSequence * COIN;
                    continue;
                }

                COutPoint prevout = tx.vin[i].prevout;
                CTransaction txPrev;
                uint256 hashBlock;
                assert(GetTransaction(prevout.hash, txPrev, hashBlock, true));
                nValueIn += txPrev.vout[prevout.n].nValue;
            }

            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                if (i == 0 && tx.IsCoinStake())
                    continue;

                nValueOut += tx.vout[i].nValue;
            }
        }

        // Rewrite money supply
        pindex->nMoneySupply = nSupplyPrev + nValueOut - nValueIn;
        nSupplyPrev = pindex->nMoneySupply;

        // Add fraudulent funds to the supply and remove any recovered funds.
        if (pindex->nHeight == Params().Zerocoin_Block_RecalculateAccumulators()) {
            LogPrintf("%s : Original money supply=%s\n", __func__, FormatMoney(pindex->nMoneySupply));

            pindex->nMoneySupply += Params().InvalidAmountFiltered();
            LogPrintf("%s : Adding filtered funds to supply + %s : supply=%s\n", __func__, FormatMoney(Params().InvalidAmountFiltered()), FormatMoney(pindex->nMoneySupply));

            CAmount nLocked = GetInvalidUTXOValue();
            pindex->nMoneySupply -= nLocked;
            LogPrintf("%s : Removing locked from supply - %s : supply=%s\n", __func__, FormatMoney(nLocked), FormatMoney(pindex->nMoneySupply));
        }

        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
    return true;
}

bool ReindexAccumulators(std::list<uint256>& listMissingCheckpoints, std::string& strError)
{
    // PIVX: recalculate Accumulator Checkpoints that failed to database properly
    if (!listMissingCheckpoints.empty()) {
        uiInterface.ShowProgress(_("Calculating missing accumulators..."), 0);
        LogPrintf("%s : finding missing checkpoints\n", __func__);

        //search the chain to see when zerocoin started
        int nZerocoinStart = Params().Zerocoin_Block_V2_Start();

        // find each checkpoint that is missing
        CBlockIndex* pindex = chainActive[nZerocoinStart];
        while (pindex && pindex->nHeight <= Params().Zerocoin_Block_Last_Checkpoint()) {
            uiInterface.ShowProgress(_("Calculating missing accumulators..."), std::max(1, std::min(99, (int)((double)(pindex->nHeight - nZerocoinStart) / (double)(chainActive.Height() - nZerocoinStart) * 100))));

            if (ShutdownRequested())
                return false;

            // find checkpoints by iterating through the blockchain beginning with the first zerocoin block
            if (pindex->nAccumulatorCheckpoint != pindex->pprev->nAccumulatorCheckpoint) {
                if (find(listMissingCheckpoints.begin(), listMissingCheckpoints.end(), pindex->nAccumulatorCheckpoint) != listMissingCheckpoints.end()) {
                    uint256 nCheckpointCalculated = 0;
                    AccumulatorMap mapAccumulators(Params().Zerocoin_Params(false));
                    if (!CalculateAccumulatorCheckpoint(pindex->nHeight, nCheckpointCalculated, mapAccumulators)) {
                        // GetCheckpoint could have terminated due to a shutdown request. Check this here.
                        if (ShutdownRequested())
                            break;
                        strError = _("Failed to calculate accumulator checkpoint");
                        return error("%s: %s", __func__, strError);
                    }

                    //check that the calculated checkpoint is what is in the index.
                    if (nCheckpointCalculated != pindex->nAccumulatorCheckpoint) {
                        LogPrintf("%s : height=%d calculated_checkpoint=%s actual=%s\n", __func__, pindex->nHeight, nCheckpointCalculated.GetHex(), pindex->nAccumulatorCheckpoint.GetHex());
                        strError = _("Calculated accumulator checkpoint is not what is recorded by block index");
                        return error("%s: %s", __func__, strError);
                    }

                    DatabaseChecksums(mapAccumulators);
                    auto it = find(listMissingCheckpoints.begin(), listMissingCheckpoints.end(), pindex->nAccumulatorCheckpoint);
                    listMissingCheckpoints.erase(it);
                }
            }
            pindex = chainActive.Next(pindex);
        }
        uiInterface.ShowProgress("", 100);
    }
    return true;
}

bool UpdateZPIVSupply(const CBlock& block, CBlockIndex* pindex, bool fJustCheck)
{
    std::list<CZerocoinMint> listMints;
    bool fFilterInvalid = pindex->nHeight >= Params().Zerocoin_Block_RecalculateAccumulators();
    BlockToZerocoinMintList(block, listMints, fFilterInvalid);
    std::list<libzerocoin::CoinDenomination> listSpends = ZerocoinSpendListFromBlock(block, fFilterInvalid);

    // Initialize zerocoin supply to the supply from previous block
    if (pindex->pprev && pindex->pprev->GetBlockHeader().nVersion > 3) {
        for (auto& denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) = pindex->pprev->GetZcMints(denom);
        }
    }

    // Track zerocoin money supply
    CAmount nAmountZerocoinSpent = 0;
    pindex->vMintDenominationsInBlock.clear();
    if (pindex->pprev) {
        std::set<uint256> setAddedToWallet;
        for (auto& m : listMints) {
            libzerocoin::CoinDenomination denom = m.GetDenomination();
            pindex->vMintDenominationsInBlock.push_back(m.GetDenomination());
            pindex->mapZerocoinSupply.at(denom)++;

            //Remove any of our own mints from the mintpool
            if (!fJustCheck && pwalletMain) {
                if (pwalletMain->IsMyMint(m.GetValue())) {
                    pwalletMain->UpdateMint(m.GetValue(), pindex->nHeight, m.GetTxHash(), m.GetDenomination());

                    // Add the transaction to the wallet
                    for (auto& tx : block.vtx) {
                        uint256 txid = tx.GetHash();
                        if (setAddedToWallet.count(txid))
                            continue;
                        if (txid == m.GetTxHash()) {
                            CWalletTx wtx(pwalletMain, tx);
                            wtx.nTimeReceived = block.GetBlockTime();
                            wtx.SetMerkleBranch(block);
                            pwalletMain->AddToWallet(wtx, false, nullptr);
                            setAddedToWallet.insert(txid);
                        }
                    }
                }
            }
        }

        for (auto& denom : listSpends) {
            pindex->mapZerocoinSupply.at(denom)--;
            nAmountZerocoinSpent += libzerocoin::ZerocoinDenominationToAmount(denom);

            // zerocoin failsafe
            if (pindex->GetZcMints(denom) < 0)
                return error("Block contains zerocoins that spend more than are in the available supply to spend");
        }
    }

    for (auto& denom : libzerocoin::zerocoinDenomList)
        LogPrint("zero", "%s coins for denomination %d pubcoin %s\n", __func__, denom, pindex->mapZerocoinSupply.at(denom));

    // Update Wrapped Serials amount
    // A one-time event where only the zPIV supply was off (due to serial duplication off-chain on main net)
    if (Params().NetworkID() == CBaseChainParams::MAIN && pindex->nHeight == Params().Zerocoin_Block_EndFakeSerial() + 1
            && pindex->GetZerocoinSupply() < Params().GetSupplyBeforeFakeSerial() + GetWrapppedSerialInflationAmount()) {
        for (auto denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
        }
    }
    return true;
}
