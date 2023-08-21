// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "sapling/sapling_operation.h"

#include "coincontrol.h"
#include "net.h" // for g_connman
#include "policy/policy.h" // for GetDustThreshold
#include "sapling/key_io_sapling.h"
#include "utilmoneystr.h"        // for FormatMoney

struct TxValues
{
    CAmount transInTotal{0};
    CAmount shieldedInTotal{0};
    CAmount transOutTotal{0};
    CAmount shieldedOutTotal{0};
    CAmount target{0};
};

SaplingOperation::SaplingOperation(const Consensus::Params& consensusParams, CWallet* _wallet) :
    wallet(_wallet),
    txBuilder(consensusParams, _wallet)
{
    assert (wallet != nullptr);
};

SaplingOperation::~SaplingOperation()
{
    delete tkeyChange;
}

OperationResult SaplingOperation::checkTxValues(TxValues& txValues, bool isFromtAddress, bool isFromShielded)
{
    assert(!isFromtAddress || txValues.shieldedInTotal == 0);
    assert(!isFromShielded || txValues.transInTotal == 0);

    if (isFromtAddress && (txValues.transInTotal < txValues.target)) {
        return errorOut(strprintf("Insufficient transparent funds, have %s, need %s",
                                  FormatMoney(txValues.transInTotal), FormatMoney(txValues.target)));
    }

    if (isFromShielded && (txValues.shieldedInTotal < txValues.target)) {
        return errorOut(strprintf("Insufficient shielded funds, have %s, need %s",
                                  FormatMoney(txValues.shieldedInTotal), FormatMoney(txValues.target)));
    }
    return OperationResult(true);
}

OperationResult loadKeysFromShieldedFrom(const CWallet* pwallet,
                                         const libzcash::SaplingPaymentAddress &addr,
                                         libzcash::SaplingExpandedSpendingKey& expskOut,
                                         uint256& ovkOut)
{
    // Get spending key for address
    libzcash::SaplingExtendedSpendingKey sk;
    if (!pwallet->GetSaplingExtendedSpendingKey(addr, sk)) {
        return errorOut("Spending key not in the wallet");
    }
    expskOut = sk.expsk;
    ovkOut = expskOut.full_viewing_key().ovk;
    return OperationResult(true);
}

TxValues calculateTarget(const std::vector<SendManyRecipient>& recipients, const CAmount& fee)
{
    TxValues txValues;
    for (const SendManyRecipient &t : recipients) {
        if (t.IsTransparent()) {
            txValues.transOutTotal += t.getAmount();
        } else {
            txValues.shieldedOutTotal += t.getAmount();
        }
    }
    txValues.target = txValues.shieldedOutTotal + txValues.transOutTotal + fee;
    return txValues;
}

OperationResult SaplingOperation::build()
{
    bool isFromtAddress = false;
    bool isFromShielded = false;

    if (coinControl) {
        // if coin control was selected it overrides any other defined configuration
        std::vector<OutPointWrapper> coins;
        coinControl->ListSelected(coins);
        // first check that every selected input is from the same type, cannot be mixed for clear privacy reasons.
        // error is thrown below if it happens, not here.
        for (const auto& coin : coins) {
            if (coin.outPoint.isTransparent) {
                isFromtAddress = true;
            } else {
                isFromShielded = true;
            }
        }
    } else {
        // Regular flow
        isFromtAddress = fromAddress.isFromTAddress();
        isFromShielded = fromAddress.isFromSapAddress();

        if (!isFromtAddress && !isFromShielded) {
            isFromtAddress = selectFromtaddrs;
            isFromShielded = selectFromShield;
        }
    }

    // It needs to have a from.
    if (!isFromtAddress && !isFromShielded) {
        return errorOut("From address parameter missing");
    }

    // Cannot be from both
    if (isFromtAddress && isFromShielded) {
        return errorOut("From address type cannot be shielded and transparent");
    }

    if (recipients.empty()) {
        return errorOut("No recipients");
    }

    if (isFromShielded && mindepth == 0) {
        return errorOut("Minconf cannot be zero when sending from shielded address");
    }

    // Check outputs to subtract fee from
    unsigned int nSubtractFeeFromAmount = 0;
    for (const SendManyRecipient& rec : recipients) {
        if (rec.IsSubtractFee()) nSubtractFeeFromAmount++;
    }

    CAmount nFeeRet = (fee > 0 ? fee : minRelayTxFee.GetFeePerK());
    int tries = 0;
    while (true) {
        // First calculate target values
        TxValues txValues = calculateTarget(recipients, nSubtractFeeFromAmount == 0 ? nFeeRet : 0);
        OperationResult result(false);
        uint256 ovk;
        if (isFromShielded) {
            // Load and select notes to spend, then return the ovk of the first note input of the transaction
            if (!(result = loadUnspentNotes(txValues, ovk))) {
                return result;
            }
        } else {
            // Get the common OVK for recovering t->shield outputs.
            // If not already databased, a new one will be generated from the HD seed.
            // It is safe to do it here, as the wallet is unlocked.
            ovk = wallet->GetSaplingScriptPubKeyMan()->getCommonOVK();
        }

        // Add outputs
        bool fFirst = true;
        for (const SendManyRecipient &t : recipients) {
            CAmount amount = t.getAmount();
            // Subtract from fee calculation
            if (t.IsSubtractFee()) {
                // Subtract fee equally from each selected recipient
                amount -= nFeeRet / nSubtractFeeFromAmount;
                if (fFirst) {
                    // first receiver pays the remainder not divisible by output count
                    fFirst = false;
                    amount -= nFeeRet % nSubtractFeeFromAmount;
                }
            }
            // Append output
            if (t.IsTransparent()) {
                txBuilder.AddTransparentOutput(CTxOut(amount, t.getScript()));
            } else {
                const auto& address = t.getSapPaymentAddr();
                assert(IsValidPaymentAddress(address));
                std::array<unsigned char, ZC_MEMO_SIZE> vMemo = {};
                if (!(result = GetMemoFromString(t.getMemo(), vMemo)))
                    return result;
                txBuilder.AddSaplingOutput(ovk, address, amount, vMemo);
            }
        }

        // If from address is a taddr, select UTXOs to spend
        // note: when spending coinbase utxos, you can only specify a single shielded addr as the change must go somewhere
        // and if there are multiple shielded addrs, we don't know where to send it.
        if (isFromtAddress && !(result = loadUtxos(txValues))) {
            return result;
        }

        const auto& retCalc = checkTxValues(txValues, isFromtAddress, isFromShielded);
        if (!retCalc) return retCalc;

        // Set change address if we are using transparent funds
        if (isFromtAddress) {
            if (!tkeyChange) {
                tkeyChange = new CReserveKey(wallet);
            }
            CPubKey vchPubKey;
            if (!tkeyChange->GetReservedKey(vchPubKey, true)) {
                return errorOut("Could not generate a taddr to use as a change address");
            }
            CTxDestination changeAddr = vchPubKey.GetID();
            txBuilder.SendChangeTo(changeAddr);
        }

        // Build the transaction
        txBuilder.SetFee(nFeeRet);
        TransactionBuilderResult txResult = txBuilder.Build(true);
        auto opTx = txResult.GetTx();

        // Check existent tx
        if (!opTx) {
            return errorOut("Failed to build transaction: " + txResult.GetError());
        }

        // Now check fee
        bool isShielded = opTx->IsShieldedTx();
        const CAmount& nFeeNeeded = isShielded ? GetShieldedTxMinFee(*opTx) :
                                                 GetMinRelayFee(opTx->GetTotalSize());
        if (nFeeNeeded <= nFeeRet) {
            // Check that the fee is not too high.
            CAmount nMaxFee = nFeeNeeded * (isShielded ? 100 : 10000);
            if (nFeeRet > nMaxFee) {
                return errorOut(strprintf("The transaction fee is too high: %s > %s", FormatMoney(nFeeRet), FormatMoney(100 * nFeeNeeded)));
            }
            // Done, enough fee included
            LogPrint(BCLog::SAPLING, "%s: spending %s to send %s with fee %s (min required %s)\n", __func__ , FormatMoney(txValues.target),
                    FormatMoney(txValues.shieldedOutTotal + txValues.transOutTotal), FormatMoney(nFeeRet), FormatMoney(nFeeNeeded));
            LogPrint(BCLog::SAPLING, "%s: transparent input: %s (to choose from)\n", __func__ , FormatMoney(txValues.transInTotal));
            LogPrint(BCLog::SAPLING, "%s: private input: %s (to choose from)\n", __func__ , FormatMoney(txValues.shieldedInTotal));
            LogPrint(BCLog::SAPLING, "%s: transparent output: %s\n", __func__ , FormatMoney(txValues.transOutTotal));
            LogPrint(BCLog::SAPLING, "%s: private output: %s\n", __func__ , FormatMoney(txValues.shieldedOutTotal));
            break;
        }
        if (fee > 0 && nFeeNeeded > fee) {
            // User selected fee is not enough
            return errorOut(strprintf("Fee set (%s) too low. Must be at least %s", FormatMoney(fee), FormatMoney(nFeeNeeded)));
        }
        // If we can't get the optimal fee after 100 tries, give up.
        if (++tries > 100) {
            return errorOut("Unable to compute optimal fee. Set manually.");
        }
        // include more fee and try again
        LogPrint(BCLog::SAPLING, "%s: incrementing fee: %s --> %s\n", __func__ , FormatMoney(nFeeRet), FormatMoney(nFeeNeeded));
        clearTx();
        nFeeRet = nFeeNeeded;
    }
    // Done
    fee = nFeeRet;

    // Clear dummy signatures/proofs and add real ones
    txBuilder.ClearProofsAndSignatures();
    TransactionBuilderResult txResult = txBuilder.ProveAndSign();
    auto opTx = txResult.GetTx();
    // Check existent tx
    if (!opTx) {
        return errorOut("Failed to build transaction: " + txResult.GetError());
    }
    finalTx = MakeTransactionRef(*opTx);
    return OperationResult(true);
}

OperationResult SaplingOperation::send(std::string& retTxHash)
{
    const CWallet::CommitResult& res = wallet->CommitTransaction(finalTx, tkeyChange, g_connman.get());
    if (res.status != CWallet::CommitStatus::OK) {
        return errorOut(res.ToString());
    }

    retTxHash = finalTx->GetHash().ToString();
    return OperationResult(true);
}

OperationResult SaplingOperation::buildAndSend(std::string& retTxHash)
{
    OperationResult res = build();
    return (res) ? send(retTxHash) : res;
}

void SaplingOperation::setFromAddress(const CTxDestination& _dest)
{
    fromAddress = FromAddress(_dest);
}

void SaplingOperation::setFromAddress(const libzcash::SaplingPaymentAddress& _payment)
{
    fromAddress = FromAddress(_payment);
}

SaplingOperation* SaplingOperation::setSelectTransparentCoins(const bool select, const bool _fIncludeDelegated)
{
    selectFromtaddrs = select;
    if (selectFromtaddrs) fIncludeDelegated = _fIncludeDelegated;
    return this;
};

OperationResult SaplingOperation::loadUtxos(TxValues& txValues)
{
    // If the user has selected coins to spend then, directly load them.
    // The spendability, depth and other checks should have been done on the user selection side,
    // no need to do them again.
    if (coinControl && coinControl->HasSelected()) {
        std::vector<OutPointWrapper> vCoins;
        coinControl->ListSelected(vCoins);

        std::vector<COutput> selectedUTXOInputs;
        CAmount nSelectedValue = 0;
        for (const auto& outpoint : vCoins) {
            const auto* tx = wallet->GetWalletTx(outpoint.outPoint.hash);
            if (!tx) continue;
            nSelectedValue += tx->tx->vout[outpoint.outPoint.n].nValue;
            selectedUTXOInputs.emplace_back(tx, outpoint.outPoint.n, 0, true, true, true);
        }
        return loadUtxos(txValues, selectedUTXOInputs, nSelectedValue);
    }

    // No coin control selected, let's find the utxo by our own.
    std::set<CTxDestination> destinations;
    if (fromAddress.isFromTAddress()) destinations.insert(fromAddress.fromTaddr);
    CWallet::AvailableCoinsFilter coinsFilter(fIncludeDelegated,
                                              false,
                                              true,
                                              true,
                                              &destinations,
                                              mindepth);
    if (!wallet->AvailableCoins(&transInputs, nullptr, coinsFilter)) {
        return errorOut("Insufficient funds, no available UTXO to spend");
    }

    // sort in descending order, so higher utxos appear first
    std::sort(transInputs.begin(), transInputs.end(), [](const COutput& i, const COutput& j) -> bool {
        return i.Value() > j.Value();
    });

    // Final step, append utxo to the transaction

    // Get dust threshold
    CAmount dustThreshold = GetDustThreshold(dustRelayFee);
    CAmount dustChange = -1;

    CAmount selectedUTXOAmount = 0;
    std::vector<COutput> selectedTInputs;
    for (const COutput& t : transInputs) {
        const auto& outPoint = t.tx->tx->vout[t.i];
        selectedUTXOAmount += outPoint.nValue;
        selectedTInputs.emplace_back(t);
        if (selectedUTXOAmount >= txValues.target) {
            // Select another utxo if there is change less than the dust threshold.
            dustChange = selectedUTXOAmount - txValues.target;
            if (dustChange == 0 || dustChange >= dustThreshold) {
                break;
            }
        }
    }

    // Not enough funds
    if (selectedUTXOAmount < txValues.target) {
                return errorOut(strprintf("Insufficient transparent funds, have %s, need %s",
                                  FormatMoney(selectedUTXOAmount), FormatMoney(txValues.target)));
    }

    // If there is transparent change, is it valid or is it dust?
    if (dustChange < dustThreshold && dustChange != 0) {
        return errorOut(strprintf("Insufficient transparent funds, have %s, need %s more to avoid creating invalid change output %s (dust threshold is %s)",
                                  FormatMoney(selectedUTXOAmount), FormatMoney(dustThreshold - dustChange), FormatMoney(dustChange), FormatMoney(dustThreshold)));
    }

    return loadUtxos(txValues, selectedTInputs, selectedUTXOAmount);
}

OperationResult SaplingOperation::loadUtxos(TxValues& txValues, const std::vector<COutput>& selectedUTXO, const CAmount selectedUTXOAmount)
{
    transInputs = selectedUTXO;
    txValues.transInTotal = selectedUTXOAmount;

    // update the transaction with these inputs
    for (const auto& t : transInputs) {
        const auto& outPoint = t.tx->tx->vout[t.i];
        txBuilder.AddTransparentInput(COutPoint(t.tx->GetHash(), t.i), outPoint.scriptPubKey, outPoint.nValue);
    }
    return OperationResult(true);
}

/*
 * Check that the witness and nullifier of a sapling note (about to be spent) have been
 * correctly cached. If the witness is missing, return an error. If the nullifier is missing,
 * recover it from the note (now that we have the spending key).
 */
enum CacheCheckResult {OK, SPENT, INVALID};
static CacheCheckResult CheckCachedNote(CWallet* pwallet,
                                        const SaplingNoteEntry& t,
                                        const libzcash::SaplingExpandedSpendingKey& expsk)
{
    auto sspkm = pwallet->GetSaplingScriptPubKeyMan();
    CWalletTx& prevTx = pwallet->mapWallet.at(t.op.hash);
    SaplingNoteData& nd = prevTx.mapSaplingNoteData.at(t.op);
    if (nd.witnesses.empty()) {
        return CacheCheckResult::INVALID;
    }
    if (nd.nullifier == nullopt) {
        const std::string& noteStr = t.op.ToString();
        LogPrintf("WARNING: nullifier not cached for note %s. Updating...\n", noteStr);
        // get the nullifier from the note and update the cache
        const auto& witness = nd.witnesses.front();
        const Optional<uint256> nf = t.note.nullifier(expsk.full_viewing_key(), witness.position());
        // check that it's valid
        if (nf == nullopt) {
            LogPrintf("ERROR: Unable to recover nullifier for note %s.\n", noteStr);
            return CacheCheckResult::INVALID;
        }
        WITH_LOCK(pwallet->cs_wallet, sspkm->UpdateSaplingNullifierNoteMap(nd, t.op, nf));
        // re-check the spent status
        if (sspkm->IsSaplingSpent(*(nd.nullifier))) {
            LogPrintf("Removed note %s as it appears to be already spent.\n", noteStr);
            prevTx.MarkDirty();
            WalletBatch(pwallet->GetDBHandle(), "r+").WriteTx(prevTx);
            pwallet->NotifyTransactionChanged(pwallet, t.op.hash, CT_UPDATED);
            return CacheCheckResult::SPENT;
        }
    }
    return CacheCheckResult::OK;
}

OperationResult SaplingOperation::loadUnspentNotes(TxValues& txValues, uint256& ovk)
{
    shieldedInputs.clear();
    auto sspkm = wallet->GetSaplingScriptPubKeyMan();
    // if we already have selected the notes, let's directly set them.
    bool hasCoinControl = coinControl && coinControl->HasSelected();
    if (hasCoinControl) {
        std::vector<OutPointWrapper> vCoins;
        coinControl->ListSelected(vCoins);

        // Converting outpoint wrapper to sapling outpoints
        std::vector<SaplingOutPoint> vSaplingOutpoints;
        vSaplingOutpoints.reserve(vCoins.size());
        for (const auto& outpoint : vCoins) {
            vSaplingOutpoints.emplace_back(outpoint.outPoint.hash, outpoint.outPoint.n);
        }

        sspkm->GetNotes(vSaplingOutpoints, shieldedInputs);

        if (shieldedInputs.empty()) {
            return errorOut("Insufficient funds, no available notes to spend");
        }
    } else {
        // If we don't have coinControl then let's find the notes
        sspkm->GetFilteredNotes(shieldedInputs, fromAddress.fromSapAddr, mindepth);
        if (shieldedInputs.empty()) {
            // Just to notify the user properly, check if the wallet has notes with less than the min depth
            std::vector<SaplingNoteEntry> _shieldedInputs;
            sspkm->GetFilteredNotes(_shieldedInputs, fromAddress.fromSapAddr, 0);
            return errorOut(_shieldedInputs.empty() ?
                    "Insufficient funds, no available notes to spend" :
                    "Insufficient funds, shielded HMS need at least 5 confirmations");
        }
    }

    // sort in descending order, so big notes appear first
    std::sort(shieldedInputs.begin(), shieldedInputs.end(),
              [](const SaplingNoteEntry& i, const SaplingNoteEntry& j) -> bool {
                  return i.note.value() > j.note.value();
              });

    // Now select the notes that we are going to use.
    std::vector<SaplingOutPoint> ops;
    std::vector<libzcash::SaplingNote> notes;
    std::vector<libzcash::SaplingExpandedSpendingKey> spendingKeys;
    txValues.shieldedInTotal = 0;
    CAmount dustThreshold = GetShieldedDustThreshold(dustRelayFee);
    CAmount dustChange = -1;
    for (const auto& t : shieldedInputs) {
        // Get the spending key for the address.
        libzcash::SaplingExpandedSpendingKey expsk;
        uint256 ovkIn;
        auto resLoadKeys = loadKeysFromShieldedFrom(wallet, t.address, expsk, ovkIn);
        if (!resLoadKeys) return resLoadKeys;

        // If the noteData is not properly cached, for whatever reason,
        // try to update it here, now that we have the spending key.
        CacheCheckResult res = CheckCachedNote(wallet, t, expsk);
        if (res == CacheCheckResult::INVALID) {
            // This should never happen. User would be forced to zap.
            LogPrintf("ERROR: Witness/Nullifier invalid for note %s. Restart with --zapwallettxes\n", t.op.ToString());
            return errorOut("Note cache corrupt. Try \"Recover transactions\" (Settings-->Debug-->\"wallet repair\")");
        } else if (res == CacheCheckResult::SPENT) {
            // note was already spent, don't include it in the inputs
            continue;
        }

        // Return ovk to be used in the outputs
        if (ovk.IsNull()) {
            ovk = ovkIn;
        }

        // Load data
        spendingKeys.emplace_back(expsk);
        ops.emplace_back(t.op);
        notes.emplace_back(t.note);
        txValues.shieldedInTotal += t.note.value();
        if (!hasCoinControl && txValues.shieldedInTotal >= txValues.target) {
            // coin control selection by pass this check, uses all the selected notes.
            // Select another note if there is change less than the dust threshold.
            dustChange = txValues.shieldedInTotal - txValues.target;
            if (dustChange == 0 || dustChange >= dustThreshold) {
                break;
            }
        }
    }

    // Not enough funds
    if (txValues.shieldedInTotal < txValues.target) {
                return errorOut(strprintf("Insufficient shielded funds, have %s, need %s",
                                  FormatMoney(txValues.shieldedInTotal), FormatMoney(txValues.target)));
    }

    // Fetch Sapling anchor and witnesses
    uint256 anchor;
    std::vector<Optional<SaplingWitness>> witnesses;
    wallet->GetSaplingScriptPubKeyMan()->GetSaplingNoteWitnesses(ops, witnesses, anchor);

    // Add Sapling spends
    for (size_t i = 0; i < notes.size(); i++) {
        if (!witnesses[i]) {
            return errorOut("Missing witness for Sapling note");
        }
        txBuilder.AddSaplingSpend(spendingKeys[i], notes[i], anchor, witnesses[i].get());
    }

    return OperationResult(true);
}

OperationResult GetMemoFromString(const std::string& s, std::array<unsigned char, ZC_MEMO_SIZE>& memoRet)
{
    memoRet.fill(0x00);
    // default memo (no_memo), see section 5.5 of the protocol spec
    if (s.empty()) {
        memoRet[0] = 0xF6;
        return OperationResult(true);
    }
    // non-empty memo
    std::vector<unsigned char> rawMemo(s.begin(), s.end());
    const size_t sizeMemo = rawMemo.size();
    if (sizeMemo > ZC_MEMO_SIZE) {
        return errorOut(strprintf("Memo size of %d is too big, maximum allowed is %d", sizeMemo, ZC_MEMO_SIZE));
    }
    // copy vector into array
    for (unsigned int i = 0; i < sizeMemo; i++) {
        memoRet[i] = rawMemo[i];
    }
    return OperationResult(true);
}

OperationResult CheckTransactionSize(std::vector<SendManyRecipient>& recipients, bool fromTaddr)
{
    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::TxVersion::SAPLING;
    unsigned int max_tx_size = MAX_TX_SIZE_AFTER_SAPLING;

    // As a sanity check, estimate and verify that the size of the transaction will be valid.
    // Depending on the input notes, the actual tx size may turn out to be larger and perhaps invalid.
    size_t nTransparentOuts = 0;
    for (const auto& t : recipients) {
        if (t.IsTransparent()) {
            nTransparentOuts++;
            continue;
        }
        if (IsValidPaymentAddress(t.getSapPaymentAddr())) {
            mtx.sapData->vShieldedOutput.emplace_back();
        } else {
            return errorOut(strprintf("invalid recipient shielded address %s",
                    KeyIO::EncodePaymentAddress(t.getSapPaymentAddr())));
        }
    }
    CTransaction tx(mtx);
    size_t txsize = tx.GetTotalSize() + CTXOUT_REGULAR_SIZE * nTransparentOuts;
    if (fromTaddr) {
        txsize += CTXIN_SPEND_DUST_SIZE;
        txsize += CTXOUT_REGULAR_SIZE;      // There will probably be taddr change
    }
    if (txsize > max_tx_size) {
        return errorOut(strprintf("Too many outputs, size of raw transaction would be larger than limit of %d bytes", max_tx_size));
    }
    return OperationResult(true);
}
