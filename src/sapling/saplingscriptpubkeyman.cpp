// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "sapling/saplingscriptpubkeyman.h"

#include "chain.h" // for CBlockIndex
#include "primitives/transaction.h"
#include "validation.h" // for ReadBlockFromDisk()

void SaplingScriptPubKeyMan::AddToSaplingSpends(const uint256& nullifier, const uint256& wtxid)
{
    AssertLockHeld(wallet->cs_wallet);
    mapTxSaplingNullifiers.emplace(nullifier, wtxid);

    std::pair<TxNullifiers::iterator, TxNullifiers::iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);
    wallet->SyncMetaDataN(range);
}

bool SaplingScriptPubKeyMan::IsSaplingSpent(const SaplingOutPoint& op) const
{
    for (auto& i : mapSaplingNullifiersToNotes) {
        SaplingOutPoint iOp = i.second;
        if (iOp == op) {
            return IsSaplingSpent(i.first);
        }
    }
    return false;
}

bool SaplingScriptPubKeyMan::IsSaplingSpent(const uint256& nullifier) const {
    LOCK(wallet->cs_wallet); // future: move to AssertLockHeld()
    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);

    for (TxNullifiers::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = wallet->mapWallet.find(wtxid);
        if (mit != wallet->mapWallet.end() && mit->second.GetDepthInMainChain() >= 0) {
            return true; // Spent
        }
    }
    return false;
}

void SaplingScriptPubKeyMan::UpdateSaplingNullifierNoteMapForBlock(const CBlock *pblock) {
    LOCK(wallet->cs_wallet);

    for (const auto& tx : pblock->vtx) {
        auto it = wallet->mapWallet.find(tx->GetHash());
        if (it != wallet->mapWallet.end()) {
            UpdateSaplingNullifierNoteMapWithTx(it->second);
        }
    }
}

// Updates noteData and mapSaplingNullifiersToNotes directly
void SaplingScriptPubKeyMan::UpdateSaplingNullifierNoteMap(SaplingNoteData& nd, const SaplingOutPoint& op, const Optional<uint256>& nullifier)
{
    AssertLockHeld(wallet->cs_wallet);
    nd.nullifier = nullifier;
    if (nullifier) mapSaplingNullifiersToNotes[*nullifier] = op;
}

/**
 * Update mapSaplingNullifiersToNotes, computing the nullifier from a cached witness if necessary.
 */
void SaplingScriptPubKeyMan::UpdateSaplingNullifierNoteMapWithTx(CWalletTx& wtx) {
    LOCK(wallet->cs_wallet);

    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        const SaplingOutPoint& op = item.first;
        SaplingNoteData& nd = item.second;

        if (nd.witnesses.empty() || !nd.IsMyNote()) {
            // If there are no witnesses, erase the nullifier and associated mapping.
            if (nd.nullifier) {
                mapSaplingNullifiersToNotes.erase(item.second.nullifier.get());
            }
            nd.nullifier = nullopt;
        } else {
            const libzcash::SaplingIncomingViewingKey& ivk = *(nd.ivk);
            uint64_t position = nd.witnesses.front().position();
            auto extfvk = wallet->mapSaplingFullViewingKeys.at(ivk);
            OutputDescription output = wtx.tx->sapData->vShieldedOutput[op.n];
            auto optPlaintext = libzcash::SaplingNotePlaintext::decrypt(output.encCiphertext, ivk, output.ephemeralKey, output.cmu);
            if (!optPlaintext) {
                // An item in mapSaplingNoteData must have already been successfully decrypted,
                // otherwise the item would not exist in the first place.
                assert(false);
            }
            auto optNote = optPlaintext.get().note(ivk);
            if (!optNote) {
                assert(false);
            }
            auto optNullifier = optNote.get().nullifier(extfvk.fvk, position);
            if (!optNullifier) {
                // This should not happen.  If it does, maybe the position has been corrupted or miscalculated?
                assert(false);
            }
            UpdateSaplingNullifierNoteMap(nd, op, optNullifier.get());
        }
    }
}

/**
 * Update mapSaplingNullifiersToNotes with the cached nullifiers in this tx.
 */
void SaplingScriptPubKeyMan::UpdateNullifierNoteMapWithTx(const CWalletTx& wtx)
{
    {
        LOCK(wallet->cs_wallet);
        for (const mapSaplingNoteData_t::value_type& item : wtx.mapSaplingNoteData) {
            if (item.second.nullifier) {
                mapSaplingNullifiersToNotes[*item.second.nullifier] = item.first;
            }
        }
    }
}

template<typename NoteDataMap>
void CopyPreviousWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // skip externally sent notes
        if (!nd->IsMyNote()) continue;
        // Only increment witnesses that are behind the current height
        if (nd->witnessHeight < indexHeight) {
            // Check the validity of the cache
            // The only time a note witnessed above the current height
            // would be invalid here is during a reindex when blocks
            // have been decremented, and we are incrementing the blocks
            // immediately after.
            assert(nWitnessCacheSize >= (int64_t) nd->witnesses.size());
            // Witnesses being incremented should always be either -1
            // (never incremented or decremented) or one below indexHeight
            assert((nd->witnessHeight == -1) || (nd->witnessHeight == indexHeight - 1));
            // Copy the witness for the previous block if we have one
            if (nd->witnesses.size() > 0) {
                nd->witnesses.push_front(nd->witnesses.front());
            }
            if (nd->witnesses.size() > WITNESS_CACHE_SIZE) {
                nd->witnesses.pop_back();
            }
        }
    }
}

void AppendNoteCommitment(SaplingNoteData* nd, int indexHeight, int64_t nWitnessCacheSize, const uint256& note_commitment)
{
    // skip externally sent notes
    if (!nd->IsMyNote()) return;
    // No empty witnesses can reach here. Before any append, the note must be already witnessed.
    if (nd->witnessHeight < indexHeight && nd->witnesses.size() > 0) {
        // Check the validity of the cache
        // See comment in CopyPreviousWitnesses about validity.
        assert(nWitnessCacheSize >= (int64_t) nd->witnesses.size());
        nd->witnesses.front().append(note_commitment);
    }
}

template<typename Witness>
void WitnessNoteIfMine(SaplingNoteData* nd,
                       int indexHeight,
                       int64_t nWitnessCacheSize,
                       const Witness& witness)
{
    assert(nd);
    // skip externally sent and already witnessed notes
    if (!nd->IsMyNote() || nd->witnessHeight >= indexHeight) return;
    if (!nd->witnesses.empty()) {
        // We think this can happen because we write out the
        // witness cache state after every block increment or
        // decrement, but the block index itself is written in
        // batches. So if the node crashes in between these two
        // operations, it is possible for IncrementNoteWitnesses
        // to be called again on previously-cached blocks. This
        // doesn't affect existing cached notes because of the
        // NoteData::witnessHeight checks. See #1378 for details.
        LogPrintf("Inconsistent witness cache state found\n- Cache size: %d\n- Top (height %d): %s\n- New (height %d): %s\n",
                  nd->witnesses.size(), nd->witnessHeight,
                  nd->witnesses.front().root().GetHex(),
                  indexHeight,
                  witness.root().GetHex());
        nd->witnesses.clear();
    }
    nd->witnesses.push_front(witness);
    // Set height to one less than pindex so it gets incremented
    nd->witnessHeight = indexHeight - 1;
    // Check the validity of the cache
    assert(nWitnessCacheSize >= (int64_t) nd->witnesses.size());
}

template<typename NoteDataMap>
void UpdateWitnessHeights(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // skip externally sent notes
        if (!nd->IsMyNote()) continue;
        if (nd->witnessHeight < indexHeight) {
            nd->witnessHeight = indexHeight;
            // Check the validity of the cache
            // See comment in CopyPreviousWitnesses about validity.
            assert(nWitnessCacheSize >= (int64_t) nd->witnesses.size());
        }
    }
}

void SaplingScriptPubKeyMan::IncrementNoteWitnesses(const CBlockIndex* pindex,
                                                    const CBlock* pblock,
                                                    SaplingMerkleTree& saplingTreeRes)
{
    LOCK(wallet->cs_wallet);
    int chainHeight = pindex->nHeight;

    // Set the update cache flag.
    int64_t prevWitCacheSize = nWitnessCacheSize;
    if (nWitnessCacheSize < WITNESS_CACHE_SIZE) {
        nWitnessCacheSize += 1;
        nWitnessCacheNeedsUpdate = true;
    }

    // 1) Loop over the block txs and gather the note commitments ordered.
    // If the wtx is from this wallet, witness it and append the following block note commitments on top.
    std::vector<uint256> noteCommitments;
    std::vector<std::pair<CWalletTx*, SaplingNoteData*>> inBlockArrivingNotes;
    for (const auto& tx : pblock->vtx) {
        if (!tx->IsShieldedTx()) continue;

        const auto& hash = tx->GetHash();
        auto it = wallet->mapWallet.find(hash);
        bool txIsOurs = it != wallet->mapWallet.end();

        for (uint32_t i = 0; i < tx->sapData->vShieldedOutput.size(); i++) {
            const auto& cmu = tx->sapData->vShieldedOutput[i].cmu;
            noteCommitments.emplace_back(cmu);

            // Append note commitment to the in-block wallet's notes.
            // This is processed here because we already looked for the wtx on
            // the WitnessNoteIfMine call and only need to append the follow-up block notes,
            // not every block note (check below).
            for (auto& item : inBlockArrivingNotes) {
                ::AppendNoteCommitment(item.second, chainHeight, nWitnessCacheSize, cmu);
            }

            // If tx is from this wallet, try to witness the note for the first time (if exists).
            // And add it to the in-block arriving txs.
            saplingTreeRes.append(cmu);
            if (txIsOurs) {
                CWalletTx* wtx = &it->second;
                auto ndIt = wtx->mapSaplingNoteData.find({hash, i});
                if (ndIt != wtx->mapSaplingNoteData.end()) {
                    SaplingNoteData* nd = &ndIt->second;
                    ::WitnessNoteIfMine(nd, chainHeight, nWitnessCacheSize, saplingTreeRes.witness());
                    inBlockArrivingNotes.emplace_back(std::make_pair(wtx, nd));
                }
            }
        }
    }

    // 2) Mark already sync wtx, so we don't process them again.
    for (auto& item : inBlockArrivingNotes) {
        ::UpdateWitnessHeights(item.first->mapSaplingNoteData, chainHeight, nWitnessCacheSize);
    }

    // 3) Loop over the shield txs in the wallet's map (excluding the wtx arriving in this block) and for each tx:
    //    a) Copy the previous witness.
    //    b) Append all new notes commitments
    //    c) Update witness last processed height
    for (auto& it : wallet->mapWallet) {
        CWalletTx& wtx = it.second;
        if (!wtx.mapSaplingNoteData.empty()) {
            // Create copy of the previous witness (verifying pre-arriving block witness cache size)
            ::CopyPreviousWitnesses(wtx.mapSaplingNoteData, chainHeight, prevWitCacheSize);

            // Append new notes commitments.
            for (auto& noteComm : noteCommitments) {
                for (auto& item : wtx.mapSaplingNoteData) {
                    AppendNoteCommitment(&(item.second), chainHeight, nWitnessCacheSize, noteComm);
                }
            }

            // Set last processed height.
            ::UpdateWitnessHeights(wtx.mapSaplingNoteData, chainHeight, nWitnessCacheSize);
        }
    }

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

template<typename NoteDataMap>
void DecrementNoteWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // skip externally sent notes
        if (!nd->IsMyNote()) continue;
        // Only decrement witnesses that are not above the current height
        if (nd->witnessHeight <= indexHeight) {
            // Check the validity of the cache
            // See comment below (this would be invalid if there were a
            // prior decrement).
            assert(nWitnessCacheSize >= (int64_t) nd->witnesses.size());
            // Witnesses being decremented should always be either -1
            // (never incremented or decremented) or equal to the height
            // of the block being removed (indexHeight)
            assert((nd->witnessHeight == -1) || (nd->witnessHeight == indexHeight));
            if (nd->witnesses.size() > 0) {
                nd->witnesses.pop_front();
            }
            // indexHeight is the height of the block being removed, so
            // the new witness cache height is one below it.
            nd->witnessHeight = indexHeight - 1;
        }
        // Check the validity of the cache
        // Technically if there are notes witnessed above the current
        // height, their cache will now be invalid (relative to the new
        // value of nWitnessCacheSize). However, this would only occur
        // during a reindex, and by the time the reindex reaches the tip
        // of the chain again, the existing witness caches will be valid
        // again.
        // We don't set nWitnessCacheSize to zero at the start of the
        // reindex because the on-disk blocks had already resulted in a
        // chain that didn't trigger the assertion below.
        if (nd->witnessHeight < indexHeight) {
            // Subtract 1 to compare to what nWitnessCacheSize will be after
            // decrementing.
            assert((nWitnessCacheSize - 1) >= (int64_t) nd->witnesses.size());
        }
    }
}

void SaplingScriptPubKeyMan::DecrementNoteWitnesses(int nChainHeight)
{
    LOCK(wallet->cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : wallet->mapWallet) {
        ::DecrementNoteWitnesses(wtxItem.second.mapSaplingNoteData, nChainHeight, nWitnessCacheSize);
    }
    nWitnessCacheSize -= 1;
    nWitnessCacheNeedsUpdate = true;
    // TODO: If nWitnessCache is zero, we need to regenerate the caches (#1302)
    if (Params().IsRegTestNet()) { // throw an error in regtest to be able to catch it from the sapling_wallet_tests.cpp unit test.
        if (nWitnessCacheSize <= 0) throw std::runtime_error("nWitnessCacheSize > 0");
    } else assert(nWitnessCacheSize > 0);

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

/**
 * Finds all output notes in the given transaction that have been sent to
 * SaplingPaymentAddresses in this wallet.
 *
 * It should never be necessary to call this method with a CWalletTx, because
 * the result of FindMySaplingNotes (for the addresses available at the time) will
 * already have been cached in CWalletTx.mapSaplingNoteData.
 */
std::pair<mapSaplingNoteData_t, SaplingIncomingViewingKeyMap> SaplingScriptPubKeyMan::FindMySaplingNotes(const CTransaction &tx) const
{
    // First check that this tx is a Shielded tx.
    if (!tx.IsShieldedTx()) {
        return {};
    }

    LOCK(wallet->cs_KeyStore);
    const uint256& hash = tx.GetHash();

    mapSaplingNoteData_t noteData;
    SaplingIncomingViewingKeyMap viewingKeysToAdd;

    // Protocol Spec: 4.19 Block Chain Scanning (Sapling)
    for (uint32_t i = 0; i < tx.sapData->vShieldedOutput.size(); ++i) {
        const OutputDescription output = tx.sapData->vShieldedOutput[i];
        for (auto it = wallet->mapSaplingFullViewingKeys.begin(); it != wallet->mapSaplingFullViewingKeys.end(); ++it) {
            libzcash::SaplingIncomingViewingKey ivk = it->first;
            auto result = libzcash::SaplingNotePlaintext::decrypt(output.encCiphertext, ivk, output.ephemeralKey, output.cmu);
            if (!result) {
                continue;
            }

            // Check if we already have it.
            Optional<libzcash::SaplingPaymentAddress> address = ivk.address(result.get().d);
            if (address && wallet->mapSaplingIncomingViewingKeys.count(address.get()) == 0) {
                viewingKeysToAdd[address.get()] = ivk;
            }
            // We don't cache the nullifier here as computing it requires knowledge of the note position
            // in the commitment tree, which can only be determined when the transaction has been mined.
            SaplingOutPoint op {hash, i};
            SaplingNoteData nd;
            nd.ivk = ivk;
            nd.amount = result->value();
            nd.address = address;
            const auto& memo = result->memo();
            // don't save empty memo (starting with 0xF6)
            if (memo[0] < 0xF6) {
                nd.memo = memo;
            }
            noteData.insert(std::make_pair(op, nd));
            break;
        }
    }

    return std::make_pair(noteData, viewingKeysToAdd);
}

std::vector<libzcash::SaplingPaymentAddress> SaplingScriptPubKeyMan::FindMySaplingAddresses(const CTransaction& tx) const
{
    LOCK(wallet->cs_KeyStore);
    std::vector<libzcash::SaplingPaymentAddress> ret;
    if (!tx.sapData) return ret;

    // Protocol Spec: 4.19 Block Chain Scanning (Sapling)
    for (const OutputDescription& output : tx.sapData->vShieldedOutput) {
        for (auto it = wallet->mapSaplingFullViewingKeys.begin(); it != wallet->mapSaplingFullViewingKeys.end(); ++it) {
            libzcash::SaplingIncomingViewingKey ivk = it->first;
            auto result = libzcash::SaplingNotePlaintext::decrypt(output.encCiphertext, ivk, output.ephemeralKey, output.cmu);
            if (!result) {
                continue;
            }
            Optional<libzcash::SaplingPaymentAddress> address = ivk.address(result.get().d);
            if (address && wallet->mapSaplingIncomingViewingKeys.count(address.get()) != 0) {
                ret.emplace_back(address.get());
            }
        }
    }
    return ret;
}

void SaplingScriptPubKeyMan::GetNotes(const std::vector<SaplingOutPoint>& saplingOutpoints,
                                      std::vector<SaplingNoteEntry>& saplingEntriesRet) const
{
    for (const auto& outpoint : saplingOutpoints) {
        const auto* wtx = wallet->GetWalletTx(outpoint.hash);
        if (!wtx) throw std::runtime_error("No transaction available for hash " + outpoint.hash.GetHex());
        const int depth = WITH_LOCK(wallet->cs_wallet, return wtx->GetDepthInMainChain(); );
        const auto& it = wtx->mapSaplingNoteData.find(outpoint);
        if (it != wtx->mapSaplingNoteData.end()) {
            const SaplingOutPoint& op = it->first;
            const SaplingNoteData& nd = it->second;

            // skip sent notes
            if (!nd.IsMyNote()) continue;

            // recover plaintext and address
            auto optNotePtAndAddress = wtx->DecryptSaplingNote(op);
            assert(static_cast<bool>(optNotePtAndAddress));

            const libzcash::SaplingIncomingViewingKey& ivk = *(nd.ivk);
            const libzcash::SaplingNotePlaintext& notePt = optNotePtAndAddress->first;
            const libzcash::SaplingPaymentAddress& pa = optNotePtAndAddress->second;
            auto note = notePt.note(ivk).get();

            saplingEntriesRet.emplace_back(op, pa, note, notePt.memo(), depth);
        }
    }
}

/**
 * Find notes in the wallet filtered by payment address, min depth and ability to spend and if the notes are locked.
 * These notes are decrypted and added to the output parameter vector, saplingEntries.
 */
void SaplingScriptPubKeyMan::GetFilteredNotes(
    std::vector<SaplingNoteEntry>& saplingEntries,
    Optional<libzcash::SaplingPaymentAddress>& address,
    int minDepth,
    bool ignoreSpent,
    bool requireSpendingKey,
    bool ignoreLocked) const
{
    std::set<libzcash::PaymentAddress> filterAddresses;

    if (address && IsValidPaymentAddress(*address)) {
        filterAddresses.insert(*address);
    }

    GetFilteredNotes(saplingEntries, filterAddresses, minDepth, INT_MAX, ignoreSpent, requireSpendingKey, ignoreLocked);
}

/**
 * Find notes in the wallet filtered by payment addresses, min depth, max depth,
 * if the note is spent, if a spending key is required, and if the notes are locked.
 * These notes are decrypted and added to the output parameter vector, saplingEntries.
 */
void SaplingScriptPubKeyMan::GetFilteredNotes(
        std::vector<SaplingNoteEntry>& saplingEntries,
        std::set<libzcash::PaymentAddress>& filterAddresses,
        int minDepth,
        int maxDepth,
        bool ignoreSpent,
        bool requireSpendingKey,
        bool ignoreLocked) const
{
    LOCK(wallet->cs_wallet);

    for (auto& p : wallet->mapWallet) {
        const CWalletTx& wtx = p.second;

        // Filter coinbase/coinstakes transactions that don't have Sapling outputs
        if ((wtx.IsCoinBase() || wtx.IsCoinStake()) && wtx.mapSaplingNoteData.empty()) {
            continue;
        }

        // Filter the transactions before checking for notes
        const int depth = wtx.GetDepthInMainChain();
        if (!IsFinalTx(wtx.tx, wallet->GetLastBlockHeight() + 1, GetAdjustedTime()) ||
            depth < minDepth || depth > maxDepth) {
            continue;
        }

        for (const auto& it : wtx.mapSaplingNoteData) {
            const SaplingOutPoint& op = it.first;
            const SaplingNoteData& nd = it.second;

            // skip sent notes
            if (!nd.IsMyNote()) continue;

            // recover plaintext and address
            auto optNotePtAndAddress = wtx.DecryptSaplingNote(op);
            assert(static_cast<bool>(optNotePtAndAddress));

            const libzcash::SaplingIncomingViewingKey& ivk = *(nd.ivk);
            const libzcash::SaplingNotePlaintext& notePt = optNotePtAndAddress->first;
            const libzcash::SaplingPaymentAddress& pa = optNotePtAndAddress->second;
            auto note = notePt.note(ivk).get();

            // skip notes which belong to a different payment address in the wallet
            if (!(filterAddresses.empty() || filterAddresses.count(pa))) {
                continue;
            }

            if (ignoreSpent && nd.nullifier && IsSaplingSpent(*nd.nullifier)) {
                continue;
            }

            // skip notes which cannot be spent
            if (requireSpendingKey && !HaveSpendingKeyForPaymentAddress(pa)) {
                continue;
            }

            // skip locked notes.
            if (ignoreLocked && wallet->IsLockedNote(op)) {
                continue;
            }

            saplingEntries.emplace_back(op, pa, note, notePt.memo(), depth);
        }
    }
}

/* Return list of available notes and locked notes grouped by sapling address. */
std::map<libzcash::SaplingPaymentAddress, std::vector<SaplingNoteEntry>> SaplingScriptPubKeyMan::ListNotes() const
{
    std::vector<SaplingNoteEntry> notes;
    Optional<libzcash::SaplingPaymentAddress> dummy = nullopt;
    GetFilteredNotes(notes, dummy, 1, true, true, false);

    std::map<libzcash::SaplingPaymentAddress, std::vector<SaplingNoteEntry>> result;
    for (const auto& note : notes) {
        result[note.address].emplace_back(std::move(note));
    }
    return result;
}

Optional<libzcash::SaplingPaymentAddress>
SaplingScriptPubKeyMan::GetAddressFromInputIfPossible(const uint256& txHash, int index) const
{
    const CWalletTx* wtx = wallet->GetWalletTx(txHash);
    if (!wtx) return nullopt;
    return GetAddressFromInputIfPossible(wtx, index);
}

Optional<libzcash::SaplingPaymentAddress>
        SaplingScriptPubKeyMan::GetAddressFromInputIfPossible(const CWalletTx* wtx, int index) const
{
    if (!wtx->tx->sapData || wtx->tx->sapData->vShieldedSpend.empty()) return nullopt;

    SpendDescription spendDesc = wtx->tx->sapData->vShieldedSpend.at(index);
    if (!IsSaplingNullifierFromMe(spendDesc.nullifier)) return nullopt;

    // Knowing that the spent note is from us, we can get the address from
    const SaplingOutPoint& outPoint = mapSaplingNullifiersToNotes.at(spendDesc.nullifier);
    const CWalletTx& txPrev = wallet->mapWallet.at(outPoint.hash);
    return txPrev.mapSaplingNoteData.at(outPoint).address;
}

bool SaplingScriptPubKeyMan::IsSaplingNullifierFromMe(const uint256& nullifier) const
{
    LOCK(wallet->cs_wallet);
    auto it = mapSaplingNullifiersToNotes.find(nullifier);
    return it != mapSaplingNullifiersToNotes.end() && wallet->mapWallet.count(it->second.hash);
}

std::set<std::pair<libzcash::PaymentAddress, uint256>> SaplingScriptPubKeyMan::GetNullifiersForAddresses(
        const std::set<libzcash::PaymentAddress> & addresses) const
{
    AssertLockHeld(wallet->cs_wallet);
    std::set<std::pair<libzcash::PaymentAddress, uint256>> nullifierSet;
    // Sapling ivk -> list of addrs map
    // (There may be more than one diversified address for a given ivk.)
    std::map<libzcash::SaplingIncomingViewingKey, std::vector<libzcash::SaplingPaymentAddress>> ivkMap;
    for (const auto& addr : addresses) {
        auto saplingAddr = boost::get<libzcash::SaplingPaymentAddress>(&addr);
        if (saplingAddr != nullptr) {
            libzcash::SaplingIncomingViewingKey ivk;
            if (wallet->GetSaplingIncomingViewingKey(*saplingAddr, ivk))
                ivkMap[ivk].push_back(*saplingAddr);
        }
    }
    for (const auto& txPair : wallet->mapWallet) {
        for (const auto & noteDataPair : txPair.second.mapSaplingNoteData) {
            const auto& noteData = noteDataPair.second;

            // Skip sent notes
            if (!noteData.IsMyNote()) continue;
            const libzcash::SaplingIncomingViewingKey& ivk = *(noteData.ivk);

            const auto& nullifier = noteData.nullifier;
            if (nullifier && ivkMap.count(ivk)) {
                for (const auto & addr : ivkMap[ivk]) {
                    nullifierSet.insert(std::make_pair(addr, nullifier.get()));
                }
            }
        }
    }
    return nullifierSet;
}

Optional<libzcash::SaplingPaymentAddress> SaplingScriptPubKeyMan::GetOutPointAddress(const CWalletTx& tx, const SaplingOutPoint& op) const
{
    auto it = tx.mapSaplingNoteData.find(op);
    if (it == tx.mapSaplingNoteData.end()) {
        return nullopt;
    }
    return it->second.address;
}

CAmount SaplingScriptPubKeyMan::GetOutPointValue(const CWalletTx& tx, const SaplingOutPoint& op) const
{
    auto it = tx.mapSaplingNoteData.find(op);
    if (it == tx.mapSaplingNoteData.end()) {
        return 0;
    }
    return it->second.amount ? *(it->second.amount) : 0;
}

Optional<std::string> SaplingScriptPubKeyMan::GetOutPointMemo(const CWalletTx& tx, const SaplingOutPoint& op) const
{
    auto it = tx.mapSaplingNoteData.find(op);
    if (it == tx.mapSaplingNoteData.end() || !static_cast<bool>(it->second.memo))
        return nullopt;
    auto& memo = *(it->second.memo);
    auto end = FindFirstNonZero(memo.rbegin(), memo.rend());
    if (memo[0] <= 0xf4) {
        std::string memoStr(memo.begin(), end.base());
        if (IsValidUTF8(memoStr)) return memoStr;
    }
    // non UTF-8 memo. Return as hex encoded raw memo.
    return HexStr(std::vector<unsigned char>(memo.begin(), end.base()));
}

Optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>>
        SaplingScriptPubKeyMan::TryToRecoverNote(const CWalletTx& tx, const SaplingOutPoint& op)
{
    const uint256& txId = tx.GetHash();
    assert(txId == op.hash);
    // Try to recover it with the ovks (either the common one, if t->shield tx, or the ones from the spends)
    std::set<uint256> ovks;
    // Get the common OVK for recovering t->shield outputs.
    // If not already databased, a new one will be generated from the HD seed (this throws an error if the
    // wallet is currently locked). As the ovk is created when the wallet is unlocked for sending a t->shield
    // tx for the first time, a failure to decode can happen only if this note was sent (from a t-addr)
    // using this wallet.dat on another computer (and never sent t->shield txes from this computer).
    if (!tx.tx->vin.empty()) {
        try {
            ovks.emplace(getCommonOVK());
        } catch (...) {
            LogPrintf("WARNING: No CommonOVK found. Some notes might not be correctly recovered. "
                      "Unlock the wallet and call 'viewshieldtransaction %s' to fix.\n", txId.ToString());
        }
    } else {
        for (const auto& spend : tx.tx->sapData->vShieldedSpend) {
            const auto& it = mapSaplingNullifiersToNotes.find(spend.nullifier);
            if (it != mapSaplingNullifiersToNotes.end()) {
                const SaplingOutPoint& prevOut = it->second;
                const CWalletTx* txPrev = wallet->GetWalletTx(prevOut.hash);
                if (!txPrev) continue;
                const auto& itPrev = txPrev->mapSaplingNoteData.find(prevOut);
                if (itPrev != txPrev->mapSaplingNoteData.end()) {
                    const SaplingNoteData& noteData = itPrev->second;
                    if (!noteData.IsMyNote()) continue;
                    libzcash::SaplingExtendedFullViewingKey extfvk;
                    if (wallet->GetSaplingFullViewingKey(*(noteData.ivk), extfvk)) {
                        ovks.emplace(extfvk.fvk.ovk);
                    }
                }
            }
        }
    }
    return tx.RecoverSaplingNote(op, ovks);
}

isminetype SaplingScriptPubKeyMan::IsMine(const CWalletTx& wtx, const SaplingOutPoint& op) const
{
    auto ndIt = wtx.mapSaplingNoteData.find(op);
    if (ndIt != wtx.mapSaplingNoteData.end() && ndIt->second.IsMyNote()) {
        const auto addr = ndIt->second.address;
        return (static_cast<bool>(addr) && HaveSpendingKeyForPaymentAddress(*addr)) ?
               ISMINE_SPENDABLE_SHIELDED : ISMINE_WATCH_ONLY_SHIELDED;
    }
    return ISMINE_NO;
}

CAmount SaplingScriptPubKeyMan::GetCredit(const CWalletTx& tx, const isminefilter& filter, const bool fUnspent) const
{
    if (!tx.tx->IsShieldedTx() || tx.tx->sapData->vShieldedOutput.empty()) {
        return 0;
    }
    CAmount nCredit = 0;
    for (int i = 0; i < (int) tx.tx->sapData->vShieldedOutput.size(); ++i) {
        SaplingOutPoint op(tx.GetHash(), i);
        if (tx.mapSaplingNoteData.find(op) == tx.mapSaplingNoteData.end()) {
            continue;
        }
        // Obtain the noteData and check if the nullifier has being spent or not
        SaplingNoteData noteData = tx.mapSaplingNoteData.at(op);

        // Skip externally sent notes
        if (!noteData.IsMyNote()) continue;

        // The nullifier could be null if the wallet was locked when the noteData was created.
        if (noteData.nullifier &&
            (fUnspent && IsSaplingSpent(*noteData.nullifier))) {
            continue; // only unspent
        }
        // todo: check if we can spend this note or not. (if not, then it's a watch only)

        nCredit += noteData.amount ? *noteData.amount : 0;
    }
    return nCredit;
}

CAmount SaplingScriptPubKeyMan::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    if (!tx.IsShieldedTx() || tx.sapData->vShieldedSpend.empty()) {
        return 0;
    }
    CAmount nDebit = 0;
    for (const SpendDescription& spend : tx.sapData->vShieldedSpend) {
        const auto &it = mapSaplingNullifiersToNotes.find(spend.nullifier);
        if (it != mapSaplingNullifiersToNotes.end()) {
            // If we have the spend nullifier, it means that this input is ours.
            // The transaction (and decrypted note data) has been added to the wallet.
            const SaplingOutPoint& op = it->second;
            auto wit = wallet->mapWallet.find(op.hash);
            assert(wit != wallet->mapWallet.end());
            const auto& wtx = wit->second;
            auto nit = wtx.mapSaplingNoteData.find(op);
            assert(nit != wtx.mapSaplingNoteData.end());
            const auto& nd = nit->second;
            assert(nd.IsMyNote());        // todo: Add watch only check.
            assert(static_cast<bool>(nd.amount));
            nDebit += *(nd.amount);
            if (!Params().GetConsensus().MoneyRange(nDebit))
                throw std::runtime_error("SaplingScriptPubKeyMan::GetDebit() : value out of range");
        }
    }
    return nDebit;
}

CAmount SaplingScriptPubKeyMan::GetShieldedChange(const CWalletTx& wtx) const
{
    if (!wtx.tx->IsShieldedTx() || wtx.tx->sapData->vShieldedOutput.empty()) {
        return 0;
    }
    const uint256& txHash = wtx.GetHash();
    CAmount nChange = 0;
    SaplingOutPoint op{txHash, 0};
    for (uint32_t pos = 0; pos < (uint32_t) wtx.tx->sapData->vShieldedOutput.size(); ++pos) {
        op.n = pos;
        auto it = wtx.mapSaplingNoteData.find(op);
        if (it == wtx.mapSaplingNoteData.end()) continue;
        const auto& nd = it->second;
        if (!nd.IsMyNote() || !static_cast<bool>(nd.address) || !static_cast<bool>(nd.amount)) continue;
        if (IsNoteSaplingChange(op, *(nd.address))) {
            nChange += *(nd.amount);
            if (!Params().GetConsensus().MoneyRange(nChange))
                throw std::runtime_error("GetShieldedChange() : value out of range");
        }
    }
    return nChange;
}

bool SaplingScriptPubKeyMan::IsNoteSaplingChange(const SaplingOutPoint& op, libzcash::SaplingPaymentAddress address) const
{
    LOCK2(wallet->cs_wallet, wallet->cs_KeyStore);
    std::set<libzcash::PaymentAddress> shieldedAddresses = {address};
    std::set<std::pair<libzcash::PaymentAddress, uint256>> nullifierSet = GetNullifiersForAddresses(shieldedAddresses);
    return IsNoteSaplingChange(nullifierSet, address, op);
}

bool SaplingScriptPubKeyMan::IsNoteSaplingChange(const std::set<std::pair<libzcash::PaymentAddress, uint256>> & nullifierSet,
                                  const libzcash::PaymentAddress & address,
                                  const SaplingOutPoint & op) const
{
    // A Note is marked as "change" if the address that received it
    // also spent Notes in the same transaction. This will catch,
    // for instance:
    // - Change created by spending fractions of Notes (because
    //   shieldsendmany sends change to the originating shielded address).
    // - Notes sent from one address to itself.
    const auto& tx = wallet->mapWallet.at(op.hash);
    if (tx.tx->sapData) {
        for (const SpendDescription& spend : tx.tx->sapData->vShieldedSpend) {
            if (nullifierSet.count(std::make_pair(address, spend.nullifier))) {
                return true;
            }
        }
    }
    return false;
}

void SaplingScriptPubKeyMan::GetSaplingNoteWitnesses(const std::vector<SaplingOutPoint>& notes,
                                      std::vector<Optional<SaplingWitness>>& witnesses,
                                      uint256& final_anchor) const
{
    LOCK(wallet->cs_wallet);
    witnesses.resize(notes.size());
    Optional<uint256> rt;
    int i = 0;
    for (SaplingOutPoint note : notes) {
        auto it = wallet->mapWallet.find(note.hash);
        if (it != wallet->mapWallet.end()) {
            auto nit = it->second.mapSaplingNoteData.find(note);
            if (nit != it->second.mapSaplingNoteData.end() &&
                    nit->second.witnesses.size() > 0) {
                witnesses[i] = nit->second.witnesses.front();
                if (!rt) {
                    rt = witnesses[i]->root();
                } else {
                    assert(*rt == witnesses[i]->root());
                }
            }
        }
        i++;
    }
    // All returned witnesses have the same anchor
    if (rt) {
        final_anchor = *rt;
    }
}

bool SaplingScriptPubKeyMan::UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx)
{
    bool unchangedSaplingFlag = (wtxIn.mapSaplingNoteData.empty() || wtxIn.mapSaplingNoteData == wtx.mapSaplingNoteData);
    if (!unchangedSaplingFlag) {
        auto tmp = wtxIn.mapSaplingNoteData;
        // Ensure we keep any cached witnesses we may already have

        for (const std::pair <SaplingOutPoint, SaplingNoteData> nd : wtx.mapSaplingNoteData) {
            if (tmp.count(nd.first) && nd.second.witnesses.size() > 0) {
                tmp.at(nd.first).witnesses.assign(
                        nd.second.witnesses.cbegin(), nd.second.witnesses.cend());
            }
            tmp.at(nd.first).witnessHeight = nd.second.witnessHeight;
        }

        // Now copy over the updated note data
        wtx.mapSaplingNoteData = tmp;
    }

    return !unchangedSaplingFlag;
}

void SaplingScriptPubKeyMan::ClearNoteWitnessCache()
{
    LOCK(wallet->cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : wallet->mapWallet) {
        for (mapSaplingNoteData_t::value_type& item : wtxItem.second.mapSaplingNoteData) {
            item.second.witnesses.clear();
            item.second.witnessHeight = -1;
        }
    }
    nWitnessCacheSize = 0;
    nWitnessCacheNeedsUpdate = true;
}

Optional<libzcash::SaplingExtendedSpendingKey> SaplingScriptPubKeyMan::GetSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &addr) const
{
    libzcash::SaplingExtendedSpendingKey extsk;
    if (wallet->GetSaplingExtendedSpendingKey(addr, extsk)) {
        return extsk;
    } else {
        return nullopt;
    }
}

Optional<libzcash::SaplingExtendedFullViewingKey> SaplingScriptPubKeyMan::GetViewingKeyForPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr) const
{
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingExtendedFullViewingKey extfvk;

    if (wallet->GetSaplingIncomingViewingKey(addr, ivk) &&
        wallet->GetSaplingFullViewingKey(ivk, extfvk))
    {
        return extfvk;
    } else {
        return nullopt;
    }
}

//! TODO: Should be Sapling address format, SaplingPaymentAddress
// Generate a new Sapling spending key and return its public payment address
libzcash::SaplingPaymentAddress SaplingScriptPubKeyMan::GenerateNewSaplingZKey()
{
    LOCK(wallet->cs_wallet); // mapSaplingZKeyMetadata

    // Try to get the seed
    CKey seedKey;
    if (!wallet->GetKey(hdChain.GetID(), seedKey))
        throw std::runtime_error(std::string(__func__) + ": HD seed not found");

    HDSeed seed(seedKey.GetPrivKey());
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | ZIP32_HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(119 | ZIP32_HARDENED_KEY_LIMIT);

    // Derive account key at next index, skip keys already known to the wallet
    libzcash::SaplingExtendedSpendingKey xsk;
    do {
        xsk = m_32h_cth.Derive(hdChain.nExternalChainCounter | ZIP32_HARDENED_KEY_LIMIT);
        hdChain.nExternalChainCounter++; // Increment childkey index
    } while (wallet->HaveSaplingSpendingKey(xsk.ToXFVK()));

    // Update the chain model in the database
    if (!WalletBatch(wallet->GetDBHandle()).WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    auto ivk = xsk.expsk.full_viewing_key().in_viewing_key();
    CKeyMetadata metadata(nCreationTime);
    metadata.key_origin.path.push_back(32 | BIP32_HARDENED_KEY_LIMIT);
    metadata.key_origin.path.push_back(119 | BIP32_HARDENED_KEY_LIMIT);
    metadata.key_origin.path.push_back(hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
    metadata.hd_seed_id = hdChain.GetID();
    mapSaplingZKeyMetadata[ivk] = metadata;

    if (!AddSaplingZKey(xsk)) {
        throw std::runtime_error(std::string(__func__) + ": AddSaplingZKey failed");
    }
    // return default sapling payment address.
    return xsk.DefaultAddress();
}

int64_t SaplingScriptPubKeyMan::GetKeyCreationTime(const libzcash::SaplingIncomingViewingKey& ivk)
{
    auto it = mapSaplingZKeyMetadata.find(ivk);
    return it != mapSaplingZKeyMetadata.end() ? it->second.nCreateTime : 0;
}

void SaplingScriptPubKeyMan::GetConflicts(const CWalletTx& wtx, std::set<uint256>& result) const
{
    AssertLockHeld(wallet->cs_wallet);
    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range_o;

    if (wtx.tx->IsShieldedTx()) {
        for (const SpendDescription& spend : wtx.tx->sapData->vShieldedSpend) {
            const uint256& nullifier = spend.nullifier;
            if (mapTxSaplingNullifiers.count(nullifier) <= 1) {
                continue;  // No conflict if zero or one spends
            }
            range_o = mapTxSaplingNullifiers.equal_range(nullifier);
            for (TxNullifiers::const_iterator it = range_o.first; it != range_o.second; ++it) {
                result.insert(it->second);
            }
        }
    }
}

KeyAddResult SaplingScriptPubKeyMan::AddViewingKeyToWallet(const libzcash::SaplingExtendedFullViewingKey &extfvk) const {
    if (wallet->HaveSaplingSpendingKey(extfvk)) {
        return SpendingKeyExists;
    } else if (wallet->HaveSaplingFullViewingKey(extfvk.fvk.in_viewing_key())) {
        return KeyAlreadyExists;
    } else if (wallet->AddSaplingFullViewingKey(extfvk)) {
        return KeyAdded;
    } else {
        return KeyNotAdded;
    }
}

KeyAddResult SaplingScriptPubKeyMan::AddSpendingKeyToWallet(
        const Consensus::Params &params,
        const libzcash::SaplingExtendedSpendingKey &sk,
        int64_t nTime)
{
    auto extfvk = sk.ToXFVK();
    auto ivk = extfvk.fvk.in_viewing_key();
    {
        //LogPrint(BCLog::SAPLING, "Importing shielded addr %s...\n", KeyIO::EncodePaymentAddress(sk.DefaultAddress()));
        // Don't throw error in case a key is already there
        if (wallet->HaveSaplingSpendingKey(extfvk)) {
            return KeyAlreadyExists;
        } else {
            if (!wallet-> AddSaplingZKey(sk)) {
                return KeyNotAdded;
            }

            int64_t nTimeToSet;
            // Sapling addresses can't have been used in transactions prior to activation.
            if (params.vUpgrades[Consensus::UPGRADE_V5_0].nActivationHeight == Consensus::NetworkUpgrade::ALWAYS_ACTIVE) {
                nTimeToSet = nTime;
            } else {
                // TODO: Update epoch before release v5.
                // 154051200 seconds from epoch is Friday, 26 October 2018 00:00:00 GMT - definitely before Sapling activates
                nTimeToSet = std::max((int64_t) 154051200, nTime);
            }

            mapSaplingZKeyMetadata[ivk] = CKeyMetadata(nTimeToSet);
            return KeyAdded;
        }
    }
}

// Add spending key to keystore
bool SaplingScriptPubKeyMan::AddSaplingZKey(
        const libzcash::SaplingExtendedSpendingKey &sk)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata

    if (!IsEnabled()) {
        return error("%s: Sapling spkm not enabled", __func__ );
    }

    if (!AddSaplingSpendingKey(sk)) {
        return false;
    }

    if (!wallet->IsCrypted()) {
        auto ivk = sk.expsk.full_viewing_key().in_viewing_key();
        return WalletBatch(wallet->GetDBHandle()).WriteSaplingZKey(ivk, sk, mapSaplingZKeyMetadata[ivk]);
    }

    return true;
}

bool SaplingScriptPubKeyMan::AddSaplingSpendingKey(
        const libzcash::SaplingExtendedSpendingKey &sk)
{
    {
        LOCK(wallet->cs_KeyStore);
        if (!wallet->IsCrypted()) {
            return wallet->AddSaplingSpendingKey(sk); // keystore
        }

        if (wallet->IsLocked()) {
            return false;
        }

        std::vector<unsigned char> vchCryptedSecret;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto extfvk = sk.ToXFVK();
        if (!EncryptSecret(wallet->GetEncryptionKey(), vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }

        if (!AddCryptedSaplingSpendingKeyDB(extfvk, vchCryptedSecret)) {
            return false;
        }
    }
    return true;
}

// Add payment address -> incoming viewing key map entry
bool SaplingScriptPubKeyMan::AddSaplingIncomingViewingKey(
        const libzcash::SaplingIncomingViewingKey &ivk,
        const libzcash::SaplingPaymentAddress &addr)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata

    if (!wallet->AddSaplingIncomingViewingKey(ivk, addr)) {
        return false;
    }

    if (!wallet->IsCrypted()) {
        return WalletBatch(wallet->GetDBHandle()).WriteSaplingPaymentAddress(addr, ivk);
    }

    return true;
}

bool SaplingScriptPubKeyMan::EncryptSaplingKeys(CKeyingMaterial& vMasterKeyIn)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingSpendingKeys

    for (SaplingSpendingKeyMap::value_type& mSaplingSpendingKey : wallet->mapSaplingSpendingKeys) {
        const libzcash::SaplingExtendedSpendingKey &sk = mSaplingSpendingKey.second;
        CSecureDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << sk;
        CKeyingMaterial vchSecret(ss.begin(), ss.end());
        auto extfvk = sk.ToXFVK();
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, vchSecret, extfvk.fvk.GetFingerprint(), vchCryptedSecret)) {
            return false;
        }
        if (!AddCryptedSaplingSpendingKeyDB(extfvk, vchCryptedSecret)) {
            return false;
        }
    }
    wallet->mapSaplingSpendingKeys.clear();
    return true;
}

bool SaplingScriptPubKeyMan::AddCryptedSaplingSpendingKeyDB(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                           const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!wallet->AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret))
        return false;
    {
        LOCK(wallet->cs_wallet);
        if (wallet->encrypted_batch) {
            return wallet->encrypted_batch->WriteCryptedSaplingZKey(extfvk,
                                                                vchCryptedSecret,
                                                                mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        } else {
            return WalletBatch(wallet->GetDBHandle()).WriteCryptedSaplingZKey(extfvk,
                                                                    vchCryptedSecret,
                                                                    mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        }
    }
    return false;
}

bool SaplingScriptPubKeyMan::HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const
{
    libzcash::SaplingIncomingViewingKey ivk;
    libzcash::SaplingExtendedFullViewingKey extfvk;

    return wallet->GetSaplingIncomingViewingKey(zaddr, ivk) &&
           wallet->GetSaplingFullViewingKey(ivk, extfvk) &&
           wallet->HaveSaplingSpendingKey(extfvk);
}

bool SaplingScriptPubKeyMan::PaymentAddressBelongsToWallet(const libzcash::SaplingPaymentAddress &zaddr) const
{
    libzcash::SaplingIncomingViewingKey ivk;

    // If we have a SaplingExtendedSpendingKey in the wallet, then we will
    // also have the corresponding SaplingExtendedFullViewingKey.
    return wallet->GetSaplingIncomingViewingKey(zaddr, ivk) &&
           wallet->HaveSaplingFullViewingKey(ivk);
}

///////////////////// Load ////////////////////////////////////////

bool SaplingScriptPubKeyMan::LoadCryptedSaplingZKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret)
{
    return wallet->AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret);
}

bool SaplingScriptPubKeyMan::LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta)
{
    AssertLockHeld(wallet->cs_wallet); // mapSaplingZKeyMetadata
    mapSaplingZKeyMetadata[ivk] = meta;
    return true;
}

bool SaplingScriptPubKeyMan::LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key)
{
    return wallet->AddSaplingSpendingKey(key);
}

bool SaplingScriptPubKeyMan::LoadSaplingPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr,
        const libzcash::SaplingIncomingViewingKey &ivk)
{
    return wallet->AddSaplingIncomingViewingKey(ivk, addr);
}

///////////////////// Setup ///////////////////////////////////////

bool SaplingScriptPubKeyMan::SetupGeneration(const CKeyID& keyID, bool force, bool memonly)
{
    SetHDSeed(keyID, force, memonly);
    return true;
}

void SaplingScriptPubKeyMan::SetHDSeed(const CPubKey& seed, bool force, bool memonly)
{
    SetHDSeed(seed.GetID(), force, memonly);
}

void SaplingScriptPubKeyMan::SetHDSeed(const CKeyID& keyID, bool force, bool memonly)
{
    if (!hdChain.IsNull() && !force)
        throw std::runtime_error(std::string(__func__) + ": sapling trying to set a hd seed on an already created chain");

    LOCK(wallet->cs_wallet);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdChain object
    CHDChain newHdChain(HDChain::ChainCounterType::Sapling);
    if (!newHdChain.SetSeed(keyID) ) {
        throw std::runtime_error(std::string(__func__) + ": set sapling hd seed failed");
    }

    SetHDChain(newHdChain, memonly);

    // Update the commonOVK to recover t->shield notes
    commonOVK = getCommonOVKFromSeed();
    if (!memonly && !WalletBatch(wallet->GetDBHandle()).WriteSaplingCommonOVK(*commonOVK)) {
        throw std::runtime_error(std::string(__func__) + ": writing sapling commonOVK failed");
    }
}

void SaplingScriptPubKeyMan::SetHDChain(CHDChain& chain, bool memonly)
{
    LOCK(wallet->cs_wallet);
    if (chain.chainType != HDChain::ChainCounterType::Sapling)
        throw std::runtime_error(std::string(__func__) + ": trying to store an invalid chain type");

    if (!memonly && !WalletBatch(wallet->GetDBHandle()).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing sapling chain failed");

    hdChain = chain;

    // Sanity check
    if (!wallet->HaveKey(hdChain.GetID()))
        throw std::runtime_error(std::string(__func__) + ": Not found sapling seed in wallet");
}

uint256 SaplingScriptPubKeyMan::getCommonOVK()
{
    // If already loaded, return it
    if (commonOVK) return *commonOVK;

    // Else, look for it in the database
    uint256 ovk;
    if (WalletBatch(wallet->GetDBHandle()).ReadSaplingCommonOVK(ovk)) {
        commonOVK = std::move(ovk);
        return *commonOVK;
    }

    // Otherwise create one. This throws if the wallet is encrypted.
    // So we should always call this after unlocking the wallet during a spend
    // from a transparent address, or when changing/setting the HD seed.
    commonOVK = getCommonOVKFromSeed();
    if (!WalletBatch(wallet->GetDBHandle()).WriteSaplingCommonOVK(*commonOVK)) {
        throw std::runtime_error("Unable to write sapling Common OVK to database");
    }
    return *commonOVK;
}

uint256 SaplingScriptPubKeyMan::getCommonOVKFromSeed() const
{
    // Sending from a t-address, which we don't have an ovk for. Instead,
    // generate a common one from the HD seed. This ensures the data is
    // recoverable, while keeping it logically separate from the ZIP 32
    // Sapling key hierarchy, which the user might not be using.
    CKey key;
    if (!wallet->GetKey(GetHDChain().GetID(), key)) {
        throw std::runtime_error("HD seed not found, wallet must be un-locked");
    }
    HDSeed seed{key.GetPrivKey()};
    return ovkForShieldingFromTaddr(seed);
}
