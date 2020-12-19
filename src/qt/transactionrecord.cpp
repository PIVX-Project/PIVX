// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "sapling/key_io_sapling.h"
#include "wallet/wallet.h"
#include "zpivchain.h"

#include <algorithm>
#include <stdint.h>

std::string TransactionRecord::getValueOrReturnEmpty(const std::map<std::string, std::string>& mapValue, const std::string& key)
{
    const auto& it = mapValue.find(key);
    return it != mapValue.end() ? it->second : "";
}

bool TransactionRecord::decomposeCoinStake(const CWallet* wallet, const CWalletTx& wtx,
        const CAmount& nCredit, const CAmount& nDebit,
        QList<TransactionRecord>& parts)
{
    // Return if it's not a coinstake
    if (!wtx.IsCoinStake()) {
        return false;
    }

    const uint256& hash = wtx.GetHash();
    TransactionRecord sub(hash, wtx.GetTxTime(), wtx.GetTotalSize());

    if (isminetype mine = wallet->IsMine(wtx.vout[1])) {
        // Check for cold stakes.
        if (wtx.HasP2CSOutputs()) {
            sub.credit = nCredit;
            sub.debit = -nDebit;
            loadHotOrColdStakeOrContract(wallet, wtx, sub);
        } else {
            // PIV stake reward
            CTxDestination address;
            if (!ExtractDestination(wtx.vout[1].scriptPubKey, address))
                return true;

            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.type = TransactionRecord::StakeMint;
            sub.address = EncodeDestination(address);
            sub.credit = nCredit - nDebit;
        }
    } else {
        //Masternode reward
        CTxDestination destMN;
        int nIndexMN = (int) wtx.vout.size() - 1;
        if (ExtractDestination(wtx.vout[nIndexMN].scriptPubKey, destMN) && (mine = IsMine(*wallet, destMN)) ) {
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.type = TransactionRecord::MNReward;
            sub.address = EncodeDestination(destMN);
            sub.credit = wtx.vout[nIndexMN].nValue;
        }
    }

    parts.append(sub);
    return true;
}

bool TransactionRecord::decomposeZcSpendTx(const CWallet* wallet, const CWalletTx& wtx,
                                           const CAmount& nCredit, const CAmount& nDebit,
                                           QList<TransactionRecord>& parts)
{

    // Return if it's not a zc spend
    if (!wtx.HasZerocoinSpendInputs()) {
        return false;
    }

    // Basic values
    const uint256& hash = wtx.GetHash();
    int64_t nTime = wtx.GetTxTime();

    //zerocoin spend outputs
    for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
        const CTxOut& txout = wtx.vout[nOut];
        // change that was reminted as zerocoins
        if (txout.IsZerocoinMint()) {
            continue;
        }

        std::string strAddress;
        CTxDestination address;
        if (ExtractDestination(txout.scriptPubKey, address))
            strAddress = EncodeDestination(address);

        // a zerocoinspend that was sent to an address held by this wallet
        isminetype mine = wallet->IsMine(txout);
        if (mine) {
            TransactionRecord sub(hash, nTime, wtx.GetTotalSize());
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.type = TransactionRecord::RecvFromZerocoinSpend;
            sub.credit = txout.nValue;
            sub.address = (!strAddress.empty()) ? strAddress : getValueOrReturnEmpty(wtx.mapValue, "recvzerocoinspend");
            sub.idx = (int) nOut;
            parts.append(sub);
            continue;
        }
    }
    return true;
}

bool TransactionRecord::decomposeP2CS(const CWallet* wallet, const CWalletTx& wtx,
                                           const CAmount& nCredit, const CAmount& nDebit,
                                           QList<TransactionRecord>& parts)
{
    if (wtx.HasP2CSOutputs()) {
        // Delegate tx.
        TransactionRecord sub(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize());
        sub.credit = nCredit;
        sub.debit = -nDebit;
        loadHotOrColdStakeOrContract(wallet, wtx, sub, true);
        parts.append(sub);
        return true;
    } else if (wtx.HasP2CSInputs()) {
        // Delegation unlocked
        TransactionRecord sub(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize());
        loadUnlockColdStake(wallet, wtx, sub);
        parts.append(sub);
        return true;
    }
    return false;
}

/**
 * Decompose a credit transaction into a record for each received output.
 */
bool TransactionRecord::decomposeCreditTransaction(const CWallet* wallet, const CWalletTx& wtx, QList<TransactionRecord>& parts)
{
    TransactionRecord sub(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize());
    for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
        const CTxOut& txout = wtx.vout[nOut];
        isminetype mine = wallet->IsMine(txout);
        if (mine) {
            CTxDestination address;
            sub.idx = (int) nOut; // vout index
            sub.credit = txout.nValue;
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address)) {
                // Received by PIVX Address
                sub.type = TransactionRecord::RecvWithAddress;
                sub.address = EncodeDestination(address);
            } else {
                // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                sub.type = TransactionRecord::RecvFromOther;
                sub.address = getValueOrReturnEmpty(wtx.mapValue, "from");
            }
            if (wtx.IsCoinBase()) {
                // Generated
                sub.type = TransactionRecord::Generated;
            }

            parts.append(sub);
        }
    }

    if (wtx.hasSaplingData()) {
        auto sspkm = wallet->GetSaplingScriptPubKeyMan();
        for (int i = 0; i < (int) wtx.sapData->vShieldOutput.size(); ++i) {
            SaplingOutPoint out(sub.hash, i);
            auto opAddr = sspkm->GetOutPointAddress(wtx, out);
            if (opAddr) {
                // skip it if change
                if (sspkm->IsNoteSaplingChange(out, *opAddr)) {
                    continue;
                }

                sub.address = (opAddr) ? KeyIO::EncodePaymentAddress(*opAddr) : "";
                sub.type = TransactionRecord::RecvWithShieldAddress;
                sub.credit = sspkm->GetOutPointValue(wtx, out);
                sub.memo = sspkm->GetOutPointMemo(wtx, out);
                sub.idx = i;
                parts.append(sub);
            }
        }
    }

    return true;
}

bool TransactionRecord::decomposeSendToSelfTransaction(const CWalletTx& wtx, const CAmount& nCredit,
                                                       const CAmount& nDebit, bool involvesWatchAddress,
                                                       QList<TransactionRecord>& parts, const CWallet* wallet)
{
    // Payment to self tx is presented as a single record.
    TransactionRecord sub(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize());
    sub.address = "";
    CAmount nChange = wtx.GetChange();
    if (!wtx.hasSaplingData()) {
        sub.type = TransactionRecord::SendToSelf;
        // Label for payment to self
        CTxDestination address;
        if (ExtractDestination(wtx.vout[0].scriptPubKey, address)) {
            sub.address = EncodeDestination(address);
        }
    } else {
        // we know that all of the inputs and outputs are mine and that have shield data.
        // Let's see if only have transparent inputs, so we know that this is a
        // transparent -> shield transaction
        if (wtx.sapData->vShieldSpend.empty()) {
            sub.type = TransactionRecord::SendToSelfShieldAddress;
            sub.shieldCredit = wtx.GetCredit(ISMINE_SPENDABLE_SHIELD);
            nChange += wtx.GetShieldChange();

            const auto& sspkm = wallet->GetSaplingScriptPubKeyMan();
            SaplingOutPoint out(sub.hash, 0);
            auto opAddr = sspkm->GetOutPointAddress(wtx, out);
            if (opAddr) {
                sub.address = KeyIO::EncodePaymentAddress(*opAddr);
            }
            sub.memo = sspkm->GetOutPointMemo(wtx, out);
        } else {
            // we know that the inputs are shield now, let's see if
            // if we have transparent outputs. if we have then we are converting back coins,
            // from shield to transparent
            if (!wtx.vout.empty()) {
                sub.type = TransactionRecord::SendToSelfShieldToTransparent;
                // Label for payment to self
                CTxDestination address;
                if (ExtractDestination(wtx.vout[0].scriptPubKey, address)) {
                    sub.address = EncodeDestination(address);
                }
                // little hack to show the correct amount
                sub.shieldCredit = wtx.GetCredit(ISMINE_SPENDABLE_TRANSPARENT);
            } else {
                // we know that the outputs are only shield, this is purely a change address tx.
                // show only the fee.
                sub.type = TransactionRecord::SendToSelfShieldToShieldChangeAddress;
            }
        }
    }

    sub.debit = -(nDebit - nChange);
    sub.credit = nCredit - nChange;
    sub.involvesWatchAddress = involvesWatchAddress;
    parts.append(sub);
    return true;
}

bool TransactionRecord::decomposeShieldDebitTransaction(const CWallet* wallet, const CWalletTx& wtx, CAmount nTxFee,
                                                          bool involvesWatchAddress, QList<TransactionRecord>& parts)
{
    // Return early if there are no outputs.
    if (wtx.sapData->vShieldOutput.empty()) {
        return false;
    }

    TransactionRecord sub(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize());
    auto sspkm = wallet->GetSaplingScriptPubKeyMan();
    for (int i = 0; i < (int) wtx.sapData->vShieldOutput.size(); ++i) {
        SaplingOutPoint out(sub.hash, i);
        auto opAddr = sspkm->GetOutPointAddress(wtx, out);
        // skip change
        if (!opAddr || sspkm->IsNoteSaplingChange(out, *opAddr)) {
            continue;
        }
        sub.idx = i;
        sub.involvesWatchAddress = involvesWatchAddress;
        sub.type = TransactionRecord::SendToShield;
        sub.address = KeyIO::EncodePaymentAddress(*opAddr);
        sub.memo = sspkm->GetOutPointMemo(wtx, out);
        CAmount nValue = sspkm->GetOutPointValue(wtx, out);
        /* Add fee to first output */
        if (nTxFee > 0) {
            nValue += nTxFee;
            nTxFee = 0;
        }
        sub.debit = -nValue;
        parts.append(sub);
    }
    return true;
}

/**
 * Decompose wtx outputs in records.
 */
bool TransactionRecord::decomposeDebitTransaction(const CWallet* wallet, const CWalletTx& wtx,
                                                  const CAmount& nDebit, bool involvesWatchAddress,
                                                  QList<TransactionRecord>& parts)
{
    // Return early if there are no outputs.
    if (wtx.vout.empty() && wtx.sapData->vShieldOutput.empty()) {
        return false;
    }

    // GetValueOut is the sum of transparent outs and negative sapValueBalance (shield outs minus shield spends).
    // Therefore to get the sum of the whole outputs of the tx, must re-add the shield inputs spent to it
    CAmount nTxFee = nDebit - (wtx.GetValueOut() + wtx.GetDebit(ISMINE_SPENDABLE_SHIELD | ISMINE_WATCH_ONLY_SHIELD));
    unsigned int txSize = wtx.GetTotalSize();
    const uint256& txHash = wtx.GetHash();
    const int64_t txTime = wtx.GetTxTime();

    for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++) {
        const CTxOut& txout = wtx.vout[nOut];

        if (wallet->IsMine(txout)) {
            // Ignore parts sent to self, as this is usually the change
            // from a transaction sent back to our own address.
            continue;
        }

        // Create record
        TransactionRecord sub(txHash, txTime, txSize);
        sub.idx = (int) nOut;
        sub.involvesWatchAddress = involvesWatchAddress;

        CTxDestination address;
        if (ExtractDestination(txout.scriptPubKey, address)) {
            //This is most likely only going to happen when resyncing deterministic wallet without the knowledge of the
            //private keys that the change was sent to. Do not display a "sent to" here.
            if (wtx.HasZerocoinMintOutputs())
                continue;
            // Sent to PIVX Address
            sub.type = TransactionRecord::SendToAddress;
            sub.address = EncodeDestination(address);
        } else if (txout.IsZerocoinMint()){
            sub.type = TransactionRecord::ZerocoinMint;
            sub.address = getValueOrReturnEmpty(wtx.mapValue, "zerocoinmint");
            sub.credit += txout.nValue;
        } else {
            // Sent to IP, or other non-address transaction like OP_EVAL
            sub.type = TransactionRecord::SendToOther;
            sub.address = getValueOrReturnEmpty(wtx.mapValue, "to");
        }

        CAmount nValue = txout.nValue;
        /* Add fee to first output */
        if (nTxFee > 0) {
            nValue += nTxFee;
            nTxFee = 0;
        }
        sub.debit = -nValue;

        parts.append(sub);
    }

    // Decompose shield debit
    return decomposeShieldDebitTransaction(wallet, wtx, nTxFee, involvesWatchAddress, parts);
}

// Check whether all the shield inputs and outputs are from and send to this wallet
std::pair<bool, bool> areInputsAndOutputsFromAndToMe(const CWalletTx& wtx, SaplingScriptPubKeyMan* sspkm, bool& involvesWatchAddress)
{
    // Check if all the shield spends are from me
    bool allShieldSpendsFromMe = true;
    for (const auto& spend : wtx.sapData->vShieldSpend) {
        if (!sspkm->IsSaplingNullifierFromMe(spend.nullifier)) {
            allShieldSpendsFromMe = false;
            break;
        }
    }

    // Check if all the shield outputs are to me
    bool allShieldOutToMe = true;
    for (int i = 0; i < (int) wtx.sapData->vShieldOutput.size(); ++i) {
        SaplingOutPoint op(wtx.GetHash(), i);
        isminetype mine = sspkm->IsMine(wtx, op);
        if (mine & ISMINE_WATCH_ONLY_SHIELD) involvesWatchAddress = true;
        if (mine != ISMINE_SPENDABLE_SHIELD) allShieldOutToMe = false;
    }

    return std::make_pair(allShieldSpendsFromMe, allShieldOutToMe);
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx)
{
    QList<TransactionRecord> parts;
    CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
    CAmount nDebit = wtx.GetDebit(ISMINE_ALL);

    // Decompose coinstake if needed (if it's not a coinstake, the method will no perform any action).
    if (decomposeCoinStake(wallet, wtx, nCredit, nDebit, parts)) {
        return parts;
    }

    // Decompose cold staking related transactions (with the exception of cold stakes that are decoupled in decomposeCoinStake)
    // future: merge this flow with the 'credit/debit decomposing flow'.
    if (decomposeP2CS(wallet, wtx, nCredit, nDebit, parts)) {
        return parts;
    }

    // Decompose zerocoin spend tx if needed (if it's not a zc spend, the method will not perform any action)
    if (decomposeZcSpendTx(wallet, wtx, nCredit, nDebit, parts)) {
        return parts;
    }

    // Credit/Debit decomposing flow
    CAmount nNet = nCredit - nDebit;

    // Check if the tx is credit and decompose it.
    if (nNet > 0) {
        // Decouple credit tx
        if (decomposeCreditTransaction(wallet, wtx, parts)) {
            return parts;
        }
    }

    auto sspkm = wallet->GetSaplingScriptPubKeyMan();
    // As the tx is not credit, need to check if all the inputs and outputs are from and to this wallet.
    // If it's true, then it's a sendToSelf. If not, then it's an outgoing tx.

    bool involvesWatchAddress = false;
    isminetype fAllFromMe = ISMINE_SPENDABLE;
    for (const CTxIn& txin : wtx.vin) {
        isminetype mine = wallet->IsMine(txin);
        if (mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
        if (fAllFromMe > mine) fAllFromMe = mine;
    }

    isminetype fAllToMe = ISMINE_SPENDABLE;
    for (const CTxOut& txout : wtx.vout) {
        isminetype mine = wallet->IsMine(txout);
        if (mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
        if (fAllToMe > mine) fAllToMe = mine;
    }

    // Check whether all the shield spends/outputs are from or to me.
    bool allShieldSpendsFromMe, allShieldOutToMe = true;
    std::tie(allShieldSpendsFromMe, allShieldOutToMe) =
            areInputsAndOutputsFromAndToMe(wtx, sspkm, involvesWatchAddress);

    // Check if this tx is purely a payment to self.
    if (fAllFromMe && fAllToMe && allShieldOutToMe && allShieldSpendsFromMe) {
        // Single record for sendToSelf.
        if (decomposeSendToSelfTransaction(wtx, nCredit, nDebit, involvesWatchAddress, parts, wallet)) {
            return parts;
        }
    }

    // Check if the tx is debit and decompose it.
    if (fAllFromMe || wtx.HasZerocoinMintOutputs()) {
        if (decomposeDebitTransaction(wallet, wtx, nDebit, involvesWatchAddress, parts)) {
            return parts;
        }
    }

    // if we get to this point, we have a mixed debit transaction, can't break down payees.
    TransactionRecord record(wtx.GetHash(), wtx.GetTxTime(), wtx.GetTotalSize(), TransactionRecord::Other, "", nNet, 0);
    record.involvesWatchAddress = involvesWatchAddress;
    parts.append(record);
    return parts;
}

bool ExtractAddress(const CScript& scriptPubKey, bool fColdStake, std::string& addressStr) {
    CTxDestination address;
    if (!ExtractDestination(scriptPubKey, address, fColdStake)) {
        // this shouldn't happen..
        addressStr = "No available address";
        return false;
    } else {
        addressStr = EncodeDestination(
                address,
                (fColdStake ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS)
        );
        return true;
    }
}

void TransactionRecord::loadUnlockColdStake(const CWallet* wallet, const CWalletTx& wtx, TransactionRecord& record)
{
    record.involvesWatchAddress = false;

    // Get the p2cs
    const CScript* p2csScript = nullptr;
    bool isSpendable = false;

    for (const auto &input : wtx.vin) {
        const CWalletTx* tx = wallet->GetWalletTx(input.prevout.hash);
        if (tx && tx->vout[input.prevout.n].scriptPubKey.IsPayToColdStaking()) {
            p2csScript = &tx->vout[input.prevout.n].scriptPubKey;
            isSpendable = wallet->IsMine(input) & ISMINE_SPENDABLE_ALL;
            break;
        }
    }

    if (isSpendable) {
        // owner unlocked the cold stake
        record.type = TransactionRecord::P2CSUnlockOwner;
        record.debit = -(wtx.GetStakeDelegationDebit());
        record.credit = wtx.GetCredit(ISMINE_ALL);
    } else {
        // hot node watching the unlock
        record.type = TransactionRecord::P2CSUnlockStaker;
        record.debit = -(wtx.GetColdStakingDebit());
        record.credit = -(wtx.GetColdStakingCredit());
    }

    // Extract and set the owner address
    if (p2csScript) {
        ExtractAddress(*p2csScript, false, record.address);
    }
}

void TransactionRecord::loadHotOrColdStakeOrContract(
        const CWallet* wallet,
        const CWalletTx& wtx,
        TransactionRecord& record,
        bool isContract)
{
    record.involvesWatchAddress = false;

    // Get the p2cs
    CTxOut p2csUtxo;
    for (const auto & txout : wtx.vout) {
        if (txout.scriptPubKey.IsPayToColdStaking()) {
            p2csUtxo = txout;
            break;
        }
    }

    bool isSpendable = (wallet->IsMine(p2csUtxo) & ISMINE_SPENDABLE_DELEGATED);
    bool isFromMe = wallet->IsFromMe(wtx);

    if (isContract) {
        if (isSpendable && isFromMe) {
            // Wallet delegating balance
            record.type = TransactionRecord::P2CSDelegationSentOwner;
        } else if (isFromMe){
            // Wallet delegating balance and transfering ownership
            record.type = TransactionRecord::P2CSDelegationSent;
        } else {
            // Wallet receiving a delegation
            record.type = TransactionRecord::P2CSDelegation;
        }
    } else {
        // Stake
        if (isSpendable) {
            // Offline wallet receiving an stake due a delegation
            record.type = TransactionRecord::StakeDelegated;
            record.credit = wtx.GetCredit(ISMINE_SPENDABLE_DELEGATED);
            record.debit = -(wtx.GetDebit(ISMINE_SPENDABLE_DELEGATED));
        } else {
            // Online wallet receiving an stake due a received utxo delegation that won a block.
            record.type = TransactionRecord::StakeHot;
        }
    }

    // Extract and set the owner address
    ExtractAddress(p2csUtxo.scriptPubKey, false, record.address);
}

bool IsZPIVType(TransactionRecord::Type type)
{
    switch (type) {
        case TransactionRecord::StakeZPIV:
        case TransactionRecord::ZerocoinMint:
        case TransactionRecord::ZerocoinSpend:
        case TransactionRecord::RecvFromZerocoinSpend:
        case TransactionRecord::ZerocoinSpend_Change_zPiv:
        case TransactionRecord::ZerocoinSpend_FromMe:
            return true;
        default:
            return false;
    }
}

void TransactionRecord::updateStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);
    int chainHeight = chainActive.Height();

    CBlockIndex *pindex = nullptr;
    // Find the block the tx is in
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Determine transaction status

    // Update time if needed
    int64_t nTxTime = wtx.GetTxTime();
    if (time != nTxTime) time = nTxTime;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        time,
        idx);

    bool fConflicted = false;
    int depth = 0;
    bool isTrusted = wtx.IsTrusted(depth, fConflicted);
    int nBlocksToMaturity = (wtx.IsCoinBase() || wtx.IsCoinStake()) ? std::max(0, (Params().GetConsensus().nCoinbaseMaturity + 1) - depth) : 0;

    status.countsForBalance = isTrusted && !(nBlocksToMaturity > 0);
    status.cur_num_blocks = chainHeight;
    status.depth = depth;

    if (!IsFinalTx(wtx, chainHeight + 1)) {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD) {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainHeight;
        } else {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if (type == TransactionRecord::Generated ||
            type == TransactionRecord::StakeMint ||
            type == TransactionRecord::StakeZPIV ||
            type == TransactionRecord::MNReward ||
            type == TransactionRecord::StakeDelegated ||
            type == TransactionRecord::StakeHot) {

        if (nBlocksToMaturity > 0) {
            status.status = TransactionStatus::Immature;
            status.matures_in = nBlocksToMaturity;

            if (status.depth < 0 || fConflicted) {
                status.status = TransactionStatus::NotAccepted;
            }
        } else {
            status.status = TransactionStatus::Confirmed;
            status.matures_in = 0;
        }
    } else {
        if (status.depth < 0 || fConflicted) {
            status.status = TransactionStatus::Conflicted;
        } else if (status.depth == 0) {
            status.status = TransactionStatus::Unconfirmed;
        } else if (status.depth < RecommendedNumConfirmations) {
            status.status = TransactionStatus::Confirming;
        } else {
            status.status = TransactionStatus::Confirmed;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

QString TransactionRecord::getTxID() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}

bool TransactionRecord::isCoinStake() const
{
    return (type == TransactionRecord::StakeMint || type == TransactionRecord::Generated || type == TransactionRecord::StakeZPIV);
}

bool TransactionRecord::isAnyColdStakingType() const
{
    return (type == TransactionRecord::P2CSDelegation || type == TransactionRecord::P2CSDelegationSent
            || type == TransactionRecord::P2CSDelegationSentOwner
            || type == TransactionRecord::StakeDelegated || type == TransactionRecord::StakeHot
            || type == TransactionRecord::P2CSUnlockOwner || type == TransactionRecord::P2CSUnlockStaker);
}

bool TransactionRecord::isNull() const
{
    return hash.IsNull() || size == 0;
}

std::string TransactionRecord::statusToString(){
    switch (status.status){
        case TransactionStatus::Confirmed:
            return "Confirmed";
        case TransactionStatus::OpenUntilDate:
            return "OpenUntilDate";
        case TransactionStatus::OpenUntilBlock:
            return "OpenUntilBlock";
        case TransactionStatus::Unconfirmed:
            return "Unconfirmed";
        case TransactionStatus::Confirming:
            return "Confirming";
        case TransactionStatus::Conflicted:
            return "Conflicted";
        case TransactionStatus::Immature:
            return "Immature";
        case TransactionStatus::NotAccepted:
            return "Not Accepted";
        default:
            return "No status";
    }
}
