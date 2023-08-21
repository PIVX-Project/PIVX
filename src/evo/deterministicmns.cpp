// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/deterministicgms.h"

#include "bls/key_io.h"
#include "chain.h"
#include "coins.h"
#include "chainparams.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "key_io.h"
#include "guiinterface.h"
#include "gamemasterman.h" // for gamemasterman (!TODO: remove)
#include "script/standard.h"
#include "spork.h"
#include "sync.h"

#include <univalue.h>

static const std::string DB_LIST_SNAPSHOT = "dgm_S";
static const std::string DB_LIST_DIFF = "dgm_D";

std::unique_ptr<CDeterministicGMManager> deterministicGMManager;

std::string CDeterministicGMState::ToString() const
{
    CTxDestination dest;
    std::string payoutAddress = "unknown";
    std::string operatorPayoutAddress = "none";
    if (ExtractDestination(scriptPayout, dest)) {
        payoutAddress = EncodeDestination(dest);
    }
    if (ExtractDestination(scriptOperatorPayout, dest)) {
        operatorPayoutAddress = EncodeDestination(dest);
    }

    return strprintf("CDeterministicGMState(nRegisteredHeight=%d, nLastPaidHeight=%d, nPoSePenalty=%d, nPoSeRevivedHeight=%d, nPoSeBanHeight=%d, nRevocationReason=%d, ownerAddress=%s, operatorPubKey=%s, votingAddress=%s, addr=%s, payoutAddress=%s, operatorPayoutAddress=%s)",
        nRegisteredHeight, nLastPaidHeight, nPoSePenalty, nPoSeRevivedHeight, nPoSeBanHeight, nRevocationReason,
        EncodeDestination(keyIDOwner), bls::EncodePublic(Params(), pubKeyOperator.Get()), EncodeDestination(keyIDVoting), addr.ToStringIPPort(), payoutAddress, operatorPayoutAddress);
}

void CDeterministicGMState::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("service", addr.ToStringIPPort());
    obj.pushKV("registeredHeight", nRegisteredHeight);
    obj.pushKV("lastPaidHeight", nLastPaidHeight);
    obj.pushKV("PoSePenalty", nPoSePenalty);
    obj.pushKV("PoSeRevivedHeight", nPoSeRevivedHeight);
    obj.pushKV("PoSeBanHeight", nPoSeBanHeight);
    obj.pushKV("revocationReason", nRevocationReason);
    obj.pushKV("ownerAddress", EncodeDestination(keyIDOwner));
    obj.pushKV("operatorPubKey", bls::EncodePublic(Params(), pubKeyOperator.Get()));
    obj.pushKV("votingAddress", EncodeDestination(keyIDVoting));

    CTxDestination dest1;
    if (ExtractDestination(scriptPayout, dest1)) {
        obj.pushKV("payoutAddress", EncodeDestination(dest1));
    }
    CTxDestination dest2;
    if (ExtractDestination(scriptOperatorPayout, dest2)) {
        obj.pushKV("operatorPayoutAddress", EncodeDestination(dest2));
    }
}

uint64_t CDeterministicGM::GetInternalId() const
{
    // can't get it if it wasn't set yet
    assert(internalId != std::numeric_limits<uint64_t>::max());
    return internalId;
}

std::string CDeterministicGM::ToString() const
{
    return strprintf("CDeterministicGM(proTxHash=%s, collateralOutpoint=%s, nOperatorReward=%f, state=%s", proTxHash.ToString(), collateralOutpoint.ToStringShort(), (double)nOperatorReward / 100, pdgmState->ToString());
}

void CDeterministicGM::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();

    UniValue stateObj;
    pdgmState->ToJson(stateObj);

    obj.pushKV("proTxHash", proTxHash.ToString());
    obj.pushKV("collateralHash", collateralOutpoint.hash.ToString());
    obj.pushKV("collateralIndex", (int)collateralOutpoint.n);
    obj.pushKV("operatorReward", (double)nOperatorReward / 100);
    obj.pushKV("dgmstate", stateObj);
}

CDeterministicGMCPtr CDeterministicGMList::GetGM(const uint256& proTxHash) const
{
    auto p = gmMap.find(proTxHash);
    if (p == nullptr) {
        return nullptr;
    }
    return *p;
}

CDeterministicGMCPtr CDeterministicGMList::GetValidGM(const uint256& proTxHash) const
{
    auto dgm = GetGM(proTxHash);
    if (dgm && dgm->IsPoSeBanned()) {
        return nullptr;
    }
    return dgm;
}

CDeterministicGMCPtr CDeterministicGMList::GetGMByOperatorKey(const CBLSPublicKey& pubKey)
{
    for (const auto& p : gmMap) {
        if (p.second->pdgmState->pubKeyOperator.Get() == pubKey) {
            return p.second;
        }
    }
    return nullptr;
}

CDeterministicGMCPtr CDeterministicGMList::GetGMByCollateral(const COutPoint& collateralOutpoint) const
{
    return GetUniquePropertyGM(collateralOutpoint);
}

CDeterministicGMCPtr CDeterministicGMList::GetValidGMByCollateral(const COutPoint& collateralOutpoint) const
{
    auto dgm = GetGMByCollateral(collateralOutpoint);
    if (dgm && dgm->IsPoSeBanned()) {
        return nullptr;
    }
    return dgm;
}

CDeterministicGMCPtr CDeterministicGMList::GetGMByService(const CService& service) const
{
    return GetUniquePropertyGM(service);
}

CDeterministicGMCPtr CDeterministicGMList::GetGMByInternalId(uint64_t internalId) const
{
    auto proTxHash = gmInternalIdMap.find(internalId);
    if (!proTxHash) {
        return nullptr;
    }
    return GetGM(*proTxHash);
}

static int CompareByLastPaidGetHeight(const CDeterministicGM& dgm)
{
    int height = dgm.pdgmState->nLastPaidHeight;
    if (dgm.pdgmState->nPoSeRevivedHeight != -1 && dgm.pdgmState->nPoSeRevivedHeight > height) {
        height = dgm.pdgmState->nPoSeRevivedHeight;
    } else if (height == 0) {
        height = dgm.pdgmState->nRegisteredHeight;
    }
    return height;
}

static bool CompareByLastPaid(const CDeterministicGM& _a, const CDeterministicGM& _b)
{
    int ah = CompareByLastPaidGetHeight(_a);
    int bh = CompareByLastPaidGetHeight(_b);
    if (ah == bh) {
        return _a.proTxHash < _b.proTxHash;
    } else {
        return ah < bh;
    }
}
static bool CompareByLastPaid(const CDeterministicGMCPtr& _a, const CDeterministicGMCPtr& _b)
{
    return CompareByLastPaid(*_a, *_b);
}

CDeterministicGMCPtr CDeterministicGMList::GetGMPayee() const
{
    if (gmMap.size() == 0) {
        return nullptr;
    }

    CDeterministicGMCPtr best;
    ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
        if (!best || CompareByLastPaid(dgm, best)) {
            best = dgm;
        }
    });

    return best;
}

std::vector<CDeterministicGMCPtr> CDeterministicGMList::GetProjectedGMPayees(unsigned int nCount) const
{
    if (nCount > GetValidGMsCount()) {
        nCount = GetValidGMsCount();
    }

    std::vector<CDeterministicGMCPtr> result;
    result.reserve(nCount);

    ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
        result.emplace_back(dgm);
    });
    std::sort(result.begin(), result.end(), [&](const CDeterministicGMCPtr& a, const CDeterministicGMCPtr& b) {
        return CompareByLastPaid(a, b);
    });

    result.resize(nCount);

    return result;
}

std::vector<CDeterministicGMCPtr> CDeterministicGMList::CalculateQuorum(size_t maxSize, const uint256& modifier) const
{
    auto scores = CalculateScores(modifier);

    // sort is descending order
    std::sort(scores.rbegin(), scores.rend(), [](const std::pair<arith_uint256, CDeterministicGMCPtr>& a, std::pair<arith_uint256, CDeterministicGMCPtr>& b) {
        if (a.first == b.first) {
            // this should actually never happen, but we should stay compatible with how the non deterministic GMs did the sorting
            return a.second->collateralOutpoint < b.second->collateralOutpoint;
        }
        return a.first < b.first;
    });

    // take top maxSize entries and return it
    std::vector<CDeterministicGMCPtr> result;
    result.resize(std::min(maxSize, scores.size()));
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = std::move(scores[i].second);
    }
    return result;
}

std::vector<std::pair<arith_uint256, CDeterministicGMCPtr>> CDeterministicGMList::CalculateScores(const uint256& modifier) const
{
    std::vector<std::pair<arith_uint256, CDeterministicGMCPtr>> scores;
    scores.reserve(GetAllGMsCount());
    ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
        if (dgm->pdgmState->confirmedHash.IsNull()) {
            // we only take confirmed GMs into account to avoid hash grinding on the ProRegTxHash to sneak GMs into a
            // future quorums
            return;
        }
        // calculate sha256(sha256(proTxHash, confirmedHash), modifier) per GM
        // Please note that this is not a double-sha256 but a single-sha256
        // The first part is already precalculated (confirmedHashWithProRegTxHash)
        // TODO When https://github.com/bitcoin/bitcoin/pull/13191 gets backported, implement something that is similar but for single-sha256
        uint256 h;
        CSHA256 sha256;
        sha256.Write(dgm->pdgmState->confirmedHashWithProRegTxHash.begin(), dgm->pdgmState->confirmedHashWithProRegTxHash.size());
        sha256.Write(modifier.begin(), modifier.size());
        sha256.Finalize(h.begin());

        scores.emplace_back(UintToArith256(h), dgm);
    });

    return scores;
}

int CDeterministicGMList::CalcMaxPoSePenalty() const
{
    // Maximum PoSe penalty is dynamic and equals the number of registered GMs
    // It's however at least 100.
    // This means that the max penalty is usually equal to a full payment cycle
    return std::max(100, (int)GetAllGMsCount());
}

int CDeterministicGMList::CalcPenalty(int percent) const
{
    assert(percent > 0);
    return (CalcMaxPoSePenalty() * percent) / 100;
}

void CDeterministicGMList::PoSePunish(const uint256& proTxHash, int penalty, bool debugLogs)
{
    assert(penalty > 0);

    auto dgm = GetGM(proTxHash);
    if (!dgm) {
        throw(std::runtime_error(strprintf("%s: Can't find a gamemaster with proTxHash=%s", __func__, proTxHash.ToString())));
    }

    int maxPenalty = CalcMaxPoSePenalty();

    auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
    newState->nPoSePenalty += penalty;
    newState->nPoSePenalty = std::min(maxPenalty, newState->nPoSePenalty);

    if (debugLogs) {
        LogPrintf("CDeterministicGMList::%s -- punished GM %s, penalty %d->%d (max=%d)\n",
                  __func__, proTxHash.ToString(), dgm->pdgmState->nPoSePenalty, newState->nPoSePenalty, maxPenalty);
    }

    if (newState->nPoSePenalty >= maxPenalty && newState->nPoSeBanHeight == -1) {
        newState->nPoSeBanHeight = nHeight;
        if (debugLogs) {
            LogPrintf("CDeterministicGMList::%s -- banned GM %s at height %d\n",
                      __func__, proTxHash.ToString(), nHeight);
        }
    }
    UpdateGM(proTxHash, newState);
}

void CDeterministicGMList::PoSeDecrease(const uint256& proTxHash)
{
    auto dgm = GetGM(proTxHash);
    if (!dgm) {
        throw(std::runtime_error(strprintf("%s: Can't find a gamemaster with proTxHash=%s", __func__, proTxHash.ToString())));
    }
    assert(dgm->pdgmState->nPoSePenalty > 0 && dgm->pdgmState->nPoSeBanHeight == -1);

    auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
    newState->nPoSePenalty--;
    UpdateGM(proTxHash, newState);
}

CDeterministicGMListDiff CDeterministicGMList::BuildDiff(const CDeterministicGMList& to) const
{
    CDeterministicGMListDiff diffRet;

    to.ForEachGM(false, [&](const CDeterministicGMCPtr& toPtr) {
        auto fromPtr = GetGM(toPtr->proTxHash);
        if (fromPtr == nullptr) {
            diffRet.addedGMs.emplace_back(toPtr);
        } else if (fromPtr != toPtr || fromPtr->pdgmState != toPtr->pdgmState) {
            CDeterministicGMStateDiff stateDiff(*fromPtr->pdgmState, *toPtr->pdgmState);
            if (stateDiff.fields) {
                diffRet.updatedGMs.emplace(toPtr->GetInternalId(), std::move(stateDiff));
            }
        }
    });
    ForEachGM(false, [&](const CDeterministicGMCPtr& fromPtr) {
        auto toPtr = to.GetGM(fromPtr->proTxHash);
        if (toPtr == nullptr) {
            diffRet.removedMns.emplace(fromPtr->GetInternalId());
        }
    });

    // added GMs need to be sorted by internalId so that these are added in correct order when the diff is applied later
    // otherwise internalIds will not match with the original list
    std::sort(diffRet.addedGMs.begin(), diffRet.addedGMs.end(), [](const CDeterministicGMCPtr& a, const CDeterministicGMCPtr& b) {
        return a->GetInternalId() < b->GetInternalId();
    });

    return diffRet;
}

CDeterministicGMList CDeterministicGMList::ApplyDiff(const CBlockIndex* pindex, const CDeterministicGMListDiff& diff) const
{
    CDeterministicGMList result = *this;
    result.blockHash = pindex->GetBlockHash();
    result.nHeight = pindex->nHeight;

    for (const auto& id : diff.removedMns) {
        auto dgm = result.GetGMByInternalId(id);
        if (!dgm) {
            throw(std::runtime_error(strprintf("%s: can't find a removed gamemaster, id=%d", __func__, id)));
        }
        result.RemoveGM(dgm->proTxHash);
    }
    for (const auto& dgm : diff.addedGMs) {
        result.AddGM(dgm);
    }
    for (const auto& p : diff.updatedGMs) {
        auto dgm = result.GetGMByInternalId(p.first);
        result.UpdateGM(dgm, p.second);
    }

    return result;
}

void CDeterministicGMList::AddGM(const CDeterministicGMCPtr& dgm, bool fBumpTotalCount)
{
    assert(dgm != nullptr);

    if (gmMap.find(dgm->proTxHash)) {
        throw(std::runtime_error(strprintf("%s: can't add a duplicate gamemaster with the same proTxHash=%s", __func__, dgm->proTxHash.ToString())));
    }
    if (gmInternalIdMap.find(dgm->GetInternalId())) {
        throw(std::runtime_error(strprintf("%s: can't add a duplicate gamemaster with the same internalId=%d", __func__, dgm->GetInternalId())));
    }
    if (HasUniqueProperty(dgm->pdgmState->addr)) {
        throw(std::runtime_error(strprintf("%s: can't add a gamemaster with a duplicate address %s", __func__, dgm->pdgmState->addr.ToStringIPPort())));
    }
    if (HasUniqueProperty(dgm->pdgmState->keyIDOwner) || HasUniqueProperty(dgm->pdgmState->pubKeyOperator)) {
        throw(std::runtime_error(strprintf("%s: can't add a gamemaster with a duplicate key (%s or %s)", __func__, EncodeDestination(dgm->pdgmState->keyIDOwner), bls::EncodePublic(Params(), dgm->pdgmState->pubKeyOperator.Get()))));
    }

    gmMap = gmMap.set(dgm->proTxHash, dgm);
    gmInternalIdMap = gmInternalIdMap.set(dgm->GetInternalId(), dgm->proTxHash);
    AddUniqueProperty(dgm, dgm->collateralOutpoint);
    if (dgm->pdgmState->addr != CService()) {
        AddUniqueProperty(dgm, dgm->pdgmState->addr);
    }
    AddUniqueProperty(dgm, dgm->pdgmState->keyIDOwner);
    AddUniqueProperty(dgm, dgm->pdgmState->pubKeyOperator);

    if (fBumpTotalCount) {
        // nTotalRegisteredCount acts more like a checkpoint, not as a limit,
        nTotalRegisteredCount = std::max(dgm->GetInternalId() + 1, (uint64_t)nTotalRegisteredCount);
    }
}

void CDeterministicGMList::UpdateGM(const CDeterministicGMCPtr& oldDgm, const CDeterministicGMStateCPtr& pdgmState)
{
    assert(oldDgm != nullptr);

    if (HasUniqueProperty(oldDgm->pdgmState->addr) && GetUniquePropertyGM(oldDgm->pdgmState->addr)->proTxHash != oldDgm->proTxHash) {
        throw(std::runtime_error(strprintf("%s: can't update a gamemaster with a duplicate address %s", __func__, oldDgm->pdgmState->addr.ToStringIPPort())));
    }

    auto dgm = std::make_shared<CDeterministicGM>(*oldDgm);
    auto oldState = dgm->pdgmState;
    dgm->pdgmState = pdgmState;
    gmMap = gmMap.set(oldDgm->proTxHash, dgm);

    UpdateUniqueProperty(dgm, oldState->addr, pdgmState->addr);
    UpdateUniqueProperty(dgm, oldState->keyIDOwner, pdgmState->keyIDOwner);
    UpdateUniqueProperty(dgm, oldState->pubKeyOperator, pdgmState->pubKeyOperator);
}

void CDeterministicGMList::UpdateGM(const uint256& proTxHash, const CDeterministicGMStateCPtr& pdgmState)
{
    auto oldDgm = gmMap.find(proTxHash);
    if (!oldDgm) {
        throw(std::runtime_error(strprintf("%s: Can't find a gamemaster with proTxHash=%s", __func__, proTxHash.ToString())));
    }
    UpdateGM(*oldDgm, pdgmState);
}

void CDeterministicGMList::UpdateGM(const CDeterministicGMCPtr& oldDgm, const CDeterministicGMStateDiff& stateDiff)
{
    assert(oldDgm != nullptr);
    auto oldState = oldDgm->pdgmState;
    auto newState = std::make_shared<CDeterministicGMState>(*oldState);
    stateDiff.ApplyToState(*newState);
    UpdateGM(oldDgm, newState);
}

void CDeterministicGMList::RemoveGM(const uint256& proTxHash)
{
    auto dgm = GetGM(proTxHash);
    if (!dgm) {
        throw(std::runtime_error(strprintf("%s: Can't find a gamemaster with proTxHash=%s", __func__, proTxHash.ToString())));
    }
    DeleteUniqueProperty(dgm, dgm->collateralOutpoint);
    if (dgm->pdgmState->addr != CService()) {
        DeleteUniqueProperty(dgm, dgm->pdgmState->addr);
    }
    DeleteUniqueProperty(dgm, dgm->pdgmState->keyIDOwner);
    DeleteUniqueProperty(dgm, dgm->pdgmState->pubKeyOperator);

    gmMap = gmMap.erase(proTxHash);
    gmInternalIdMap = gmInternalIdMap.erase(dgm->GetInternalId());
}

CDeterministicGMManager::CDeterministicGMManager(CEvoDB& _evoDb) :
    evoDb(_evoDb)
{
}

bool CDeterministicGMManager::ProcessBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& _state, bool fJustCheck)
{
    int nHeight = pindex->nHeight;
    if (!IsDIP3Enforced(nHeight)) {
        // nothing to do
        return true;
    }

    CDeterministicGMList oldList, newList;
    CDeterministicGMListDiff diff;

    try {
        LOCK(cs);

        if (!BuildNewListFromBlock(block, pindex->pprev, _state, newList, true)) {
            // pass the state returned by the function above
            return false;
        }

        if (fJustCheck) {
            return true;
        }

        if (newList.GetHeight() == -1) {
            newList.SetHeight(nHeight);
        }

        newList.SetBlockHash(block.GetHash());

        oldList = GetListForBlock(pindex->pprev);
        diff = oldList.BuildDiff(newList);

        evoDb.Write(std::make_pair(DB_LIST_DIFF, newList.GetBlockHash()), diff);
        if ((nHeight % DISK_SNAPSHOT_PERIOD) == 0 || oldList.GetHeight() == -1) {
            evoDb.Write(std::make_pair(DB_LIST_SNAPSHOT, newList.GetBlockHash()), newList);
            gmListsCache.emplace(newList.GetBlockHash(), newList);
            LogPrintf("CDeterministicGMManager::%s -- Wrote snapshot. nHeight=%d, mapCurGMs.allGMsCount=%d\n",
                __func__, nHeight, newList.GetAllGMsCount());
        }

        diff.nHeight = pindex->nHeight;
        gmListDiffsCache.emplace(pindex->GetBlockHash(), diff);
    } catch (const std::exception& e) {
        LogPrintf("CDeterministicGMManager::%s -- internal error: %s\n", __func__, e.what());
        return _state.DoS(100, false, REJECT_INVALID, "failed-dgm-block");
    }

    // Don't hold cs while calling signals
    if (diff.HasChanges()) {
        GetMainSignals().NotifyGamemasterListChanged(false, oldList, diff);
        uiInterface.NotifyGamemasterListChanged(newList);
    }

    LOCK(cs);
    CleanupCache(nHeight);

    return true;
}

bool CDeterministicGMManager::UndoBlock(const CBlock& block, const CBlockIndex* pindex)
{
    if (!IsDIP3Enforced(pindex->nHeight)) {
        // nothing to do
        return true;
    }

    const uint256& blockHash = block.GetHash();

    CDeterministicGMList curList;
    CDeterministicGMList prevList;
    CDeterministicGMListDiff diff;
    {
        LOCK(cs);
        evoDb.Read(std::make_pair(DB_LIST_DIFF, blockHash), diff);

        if (diff.HasChanges()) {
            // need to call this before erasing
            curList = GetListForBlock(pindex);
            prevList = GetListForBlock(pindex->pprev);
        }

        gmListsCache.erase(blockHash);
        gmListDiffsCache.erase(blockHash);
    }

    if (diff.HasChanges()) {
        auto inversedDiff = curList.BuildDiff(prevList);
        GetMainSignals().NotifyGamemasterListChanged(true, curList, inversedDiff);
        uiInterface.NotifyGamemasterListChanged(prevList);
    }

    return true;
}

void CDeterministicGMManager::SetTipIndex(const CBlockIndex* pindex)
{
    LOCK(cs);
    tipIndex = pindex;
}

bool CDeterministicGMManager::BuildNewListFromBlock(const CBlock& block, const CBlockIndex* pindexPrev, CValidationState& _state, CDeterministicGMList& gmListRet, bool debugLogs)
{
    AssertLockHeld(cs);
    const auto& consensus = Params().GetConsensus();
    int nHeight = pindexPrev->nHeight + 1;

    CDeterministicGMList oldList = GetListForBlock(pindexPrev);
    CDeterministicGMList newList = oldList;
    newList.SetBlockHash(UINT256_ZERO); // we can't know the final block hash, so better not return a (invalid) block hash
    newList.SetHeight(nHeight);

    auto payee = oldList.GetGMPayee();

    // we iterate the oldList here and update the newList
    // this is only valid as long these have not diverged at this point, which is the case as long as we don't add
    // code above this loop that modifies newList
    oldList.ForEachGM(false, [&](const CDeterministicGMCPtr& dgm) {
        if (!dgm->pdgmState->confirmedHash.IsNull()) {
            // already confirmed
            return;
        }
        // this works on the previous block, so confirmation will happen one block after nGamemasterMinimumConfirmations
        // has been reached, but the block hash will then point to the block at nGamemasterMinimumConfirmations
        int nConfirmations = pindexPrev->nHeight - dgm->pdgmState->nRegisteredHeight;
        if (nConfirmations >= consensus.GamemasterCollateralMinConf()) {
            auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
            newState->UpdateConfirmedHash(dgm->proTxHash, pindexPrev->GetBlockHash());
            newList.UpdateGM(dgm->proTxHash, newState);
        }
    });

    DecreasePoSePenalties(newList);

    // we skip the coinbase
    for (int i = 1; i < (int)block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];

        if (tx.nType == CTransaction::TxType::PROREG) {
            ProRegPL pl;
            if (!GetTxPayload(tx, pl)) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
            }

            auto dgm = std::make_shared<CDeterministicGM>(newList.GetTotalRegisteredCount());
            dgm->proTxHash = tx.GetHash();

            // collateralOutpoint is either pointing to an external collateral or to the ProRegTx itself
            dgm->collateralOutpoint = pl.collateralOutpoint.hash.IsNull() ? COutPoint(tx.GetHash(), pl.collateralOutpoint.n)
                                                                          : pl.collateralOutpoint;

            // if the collateral outpoint appears in the legacy gamemaster list, remove the old node
            // !TODO: remove this when the transition to DGM is complete
            CGamemaster* old_gm = gamemasterman.Find(dgm->collateralOutpoint);
            if (old_gm) {
                old_gm->SetSpent();
                gamemasterman.CheckAndRemove();
            }

            auto replacedDgm = newList.GetGMByCollateral(dgm->collateralOutpoint);
            if (replacedDgm != nullptr) {
                // This might only happen with a ProRegTx that refers an external collateral
                // In that case the new ProRegTx will replace the old one. This means the old one is removed
                // and the new one is added like a completely fresh one, which is also at the bottom of the payment list
                newList.RemoveGM(replacedDgm->proTxHash);
                if (debugLogs) {
                    LogPrintf("CDeterministicGMManager::%s -- GM %s removed from list because collateral was used for a new ProRegTx. collateralOutpoint=%s, nHeight=%d, mapCurGMs.allGMsCount=%d\n",
                              __func__, replacedDgm->proTxHash.ToString(), dgm->collateralOutpoint.ToStringShort(), nHeight, newList.GetAllGMsCount());
                }
            }

            if (newList.HasUniqueProperty(pl.addr)) {
                return _state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-IP-address");
            }
            if (newList.HasUniqueProperty(pl.keyIDOwner)) {
                return _state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-owner-key");
            }
            if (newList.HasUniqueProperty(pl.pubKeyOperator)) {
                return _state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key");
            }

            dgm->nOperatorReward = pl.nOperatorReward;

            auto dgmState = std::make_shared<CDeterministicGMState>(pl);
            dgmState->nRegisteredHeight = nHeight;
            if (pl.addr == CService()) {
                // start in banned pdgmState as we need to wait for a ProUpServTx
                dgmState->nPoSeBanHeight = nHeight;
            }
            dgm->pdgmState = dgmState;

            newList.AddGM(dgm);

            if (debugLogs) {
                LogPrintf("CDeterministicGMManager::%s -- GM %s added at height %d: %s\n",
                    __func__, tx.GetHash().ToString(), nHeight, pl.ToString());
            }

        } else if (tx.nType == CTransaction::TxType::PROUPSERV) {
            ProUpServPL pl;
            if (!GetTxPayload(tx, pl)) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
            }

            if (newList.HasUniqueProperty(pl.addr) && newList.GetUniquePropertyGM(pl.addr)->proTxHash != pl.proTxHash) {
                return _state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
            }

            CDeterministicGMCPtr dgm = newList.GetGM(pl.proTxHash);
            if (!dgm) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
            }
            if (dgm->nOperatorReward == 0 && !pl.scriptOperatorPayout.empty()) {
                // operator payout address can not be set if the operator reward is 0
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-payee");
            }
            auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
            newState->addr = pl.addr;
            newState->scriptOperatorPayout = pl.scriptOperatorPayout;

            if (newState->nPoSeBanHeight != -1) {
                // only revive when all keys are set
                if (newState->pubKeyOperator.Get().IsValid() && !newState->keyIDVoting.IsNull() && !newState->keyIDOwner.IsNull()) {
                    newState->nPoSePenalty = 0;
                    newState->nPoSeBanHeight = -1;
                    newState->nPoSeRevivedHeight = nHeight;

                    if (debugLogs) {
                        LogPrintf("CDeterministicGMManager::%s -- GM %s revived at height %d\n",
                            __func__, pl.proTxHash.ToString(), nHeight);
                    }
                }
            }

            newList.UpdateGM(pl.proTxHash, newState);
            if (debugLogs) {
                LogPrintf("CDeterministicGMManager::%s -- GM %s updated at height %d: %s\n",
                    __func__, pl.proTxHash.ToString(), nHeight, pl.ToString());
            }

        } else if (tx.nType == CTransaction::TxType::PROUPREG) {
            ProUpRegPL pl;
            if (!GetTxPayload(tx, pl)) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
            }

            CDeterministicGMCPtr dgm = newList.GetGM(pl.proTxHash);
            if (!dgm) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
            }
            if (newList.HasUniqueProperty(pl.pubKeyOperator) && newList.GetUniquePropertyGM(pl.pubKeyOperator)->proTxHash != pl.proTxHash) {
                return _state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key");
            }
            auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
            if (newState->pubKeyOperator.Get() != pl.pubKeyOperator) {
                // reset all operator related fields and put GM into PoSe-banned state in case the operator key changes
                newState->ResetOperatorFields();
                newState->BanIfNotBanned(nHeight);
            }
            newState->pubKeyOperator.Set(pl.pubKeyOperator);
            newState->keyIDVoting = pl.keyIDVoting;
            newState->scriptPayout = pl.scriptPayout;

            newList.UpdateGM(pl.proTxHash, newState);

            if (debugLogs) {
                LogPrintf("CDeterministicGMManager::%s -- GM %s updated at height %d: %s\n",
                    __func__, pl.proTxHash.ToString(), nHeight, pl.ToString());
            }

        } else if (tx.nType == CTransaction::TxType::PROUPREV) {
            ProUpRevPL pl;
            if (!GetTxPayload(tx, pl)) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
            }

            CDeterministicGMCPtr dgm = newList.GetGM(pl.proTxHash);
            if (!dgm) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
            }
            auto newState = std::make_shared<CDeterministicGMState>(*dgm->pdgmState);
            newState->ResetOperatorFields();
            newState->BanIfNotBanned(nHeight);
            newState->nRevocationReason = pl.nReason;

            newList.UpdateGM(pl.proTxHash, newState);

            if (debugLogs) {
                LogPrintf("CDeterministicGMManager::%s -- GM %s updated at height %d: %s\n",
                    __func__, pl.proTxHash.ToString(), nHeight, pl.ToString());
            }
        } else if (tx.nType == CTransaction::TxType::LLMQCOMM) {
            llmq::LLMQCommPL pl;
            if (!GetTxPayload(tx, pl)) {
                return _state.DoS(100, false, REJECT_INVALID, "bad-qc-payload");
            }
            if (!pl.commitment.IsNull()) {
                // Double-check that the quorum index is in the active chain
                const auto& params = consensus.llmqs.at((Consensus::LLMQType)pl.commitment.llmqType);
                uint32_t quorumHeight = pl.nHeight - (pl.nHeight % params.dkgInterval);
                auto quorumIndex = pindexPrev->GetAncestor(quorumHeight);
                if (!quorumIndex || quorumIndex->GetBlockHash() != pl.commitment.quorumHash) {
                    return _state.DoS(100, false, REJECT_INVALID, "bad-qc-quorum-hash");
                }
                // Check for failed DKG participation by GMs
                HandleQuorumCommitment(pl.commitment, quorumIndex, newList, debugLogs);
            }
        }

    }

    // check if any existing GM collateral is spent by this transaction
    // we skip the coinbase
    for (int i = 1; i < (int)block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        for (const auto& in : tx.vin) {
            auto dgm = newList.GetGMByCollateral(in.prevout);
            if (dgm && dgm->collateralOutpoint == in.prevout) {
                newList.RemoveGM(dgm->proTxHash);
                if (debugLogs) {
                    LogPrintf("CDeterministicGMManager::%s -- GM %s removed from list because collateral was spent. collateralOutpoint=%s, nHeight=%d, mapCurGMs.allGMsCount=%d\n",
                              __func__, dgm->proTxHash.ToString(), dgm->collateralOutpoint.ToStringShort(), nHeight, newList.GetAllGMsCount());
                }
            }
        }
    }

    // The payee for the current block was determined by the previous block's list but it might have disappeared in the
    // current block. We still pay that GM one last time however.
    if (payee && newList.HasGM(payee->proTxHash)) {
        auto newState = std::make_shared<CDeterministicGMState>(*newList.GetGM(payee->proTxHash)->pdgmState);
        newState->nLastPaidHeight = nHeight;
        newList.UpdateGM(payee->proTxHash, newState);
    }

    gmListRet = std::move(newList);

    return true;
}

void CDeterministicGMManager::HandleQuorumCommitment(llmq::CFinalCommitment& qc, const CBlockIndex* pindexQuorum, CDeterministicGMList& gmList, bool debugLogs)
{
    // The commitment has already been validated at this point so it's safe to use members of it

    auto members = GetAllQuorumMembers((Consensus::LLMQType)qc.llmqType, pindexQuorum);

    for (size_t i = 0; i < members.size(); i++) {
        if (!gmList.HasGM(members[i]->proTxHash)) {
            continue;
        }
        if (!qc.validMembers[i]) {
            // punish GM for failed DKG participation
            // The idea is to immediately ban a GM when it fails 2 DKG sessions with only a few blocks in-between
            // If there were enough blocks between failures, the GM has a chance to recover as he reduces his penalty by 1 for every block
            // If it however fails 3 times in the timespan of a single payment cycle, it should definitely get banned
            gmList.PoSePunish(members[i]->proTxHash, gmList.CalcPenalty(66), debugLogs);
        }
    }
}

void CDeterministicGMManager::DecreasePoSePenalties(CDeterministicGMList& gmList)
{
    std::vector<uint256> toDecrease;
    toDecrease.reserve(gmList.GetValidGMsCount() / 10);
    // only iterate and decrease for valid ones (not PoSe banned yet)
    // if a GM ever reaches the maximum, it stays in PoSe banned state until revived
    gmList.ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
        if (dgm->pdgmState->nPoSePenalty > 0 && dgm->pdgmState->nPoSeBanHeight == -1) {
            toDecrease.emplace_back(dgm->proTxHash);
        }
    });

    for (const auto& proTxHash : toDecrease) {
        gmList.PoSeDecrease(proTxHash);
    }
}

CDeterministicGMList CDeterministicGMManager::GetListForBlock(const CBlockIndex* pindex)
{
    LOCK(cs);

    // Return early before enforcement
    if (!IsDIP3Enforced(pindex->nHeight)) {
        return {};
    }

    CDeterministicGMList snapshot;
    std::list<const CBlockIndex*> listDiffIndexes;

    while (true) {
        // try using cache before reading from disk
        auto itLists = gmListsCache.find(pindex->GetBlockHash());
        if (itLists != gmListsCache.end()) {
            snapshot = itLists->second;
            break;
        }

        if (evoDb.Read(std::make_pair(DB_LIST_SNAPSHOT, pindex->GetBlockHash()), snapshot)) {
            gmListsCache.emplace(pindex->GetBlockHash(), snapshot);
            break;
        }

        // no snapshot found yet, check diffs
        auto itDiffs = gmListDiffsCache.find(pindex->GetBlockHash());
        if (itDiffs != gmListDiffsCache.end()) {
            listDiffIndexes.emplace_front(pindex);
            pindex = pindex->pprev;
            continue;
        }

        CDeterministicGMListDiff diff;
        if (!evoDb.Read(std::make_pair(DB_LIST_DIFF, pindex->GetBlockHash()), diff)) {
            // no snapshot and no diff on disk means that it's initial snapshot (empty list)
            // If we get here, then this must be the block before the enforcement of DIP3.
            if (!IsActivationHeight(pindex->nHeight + 1, Params().GetConsensus(), Consensus::UPGRADE_V6_0)) {
                std::string err = strprintf("No gamemaster list data found for block %s at height %d. "
                                            "Possible corrupt database.", pindex->GetBlockHash().ToString(), pindex->nHeight);
                throw std::runtime_error(err);
            }
            snapshot = CDeterministicGMList(pindex->GetBlockHash(), -1, 0);
            gmListsCache.emplace(pindex->GetBlockHash(), snapshot);
            break;
        }

        diff.nHeight = pindex->nHeight;
        gmListDiffsCache.emplace(pindex->GetBlockHash(), std::move(diff));
        listDiffIndexes.emplace_front(pindex);
        pindex = pindex->pprev;
    }

    for (const auto& diffIndex : listDiffIndexes) {
        const auto& diff = gmListDiffsCache.at(diffIndex->GetBlockHash());
        if (diff.HasChanges()) {
            snapshot = snapshot.ApplyDiff(diffIndex, diff);
        } else {
            snapshot.SetBlockHash(diffIndex->GetBlockHash());
            snapshot.SetHeight(diffIndex->nHeight);
        }
    }

    if (tipIndex) {
        // always keep a snapshot for the tip
        if (snapshot.GetBlockHash() == tipIndex->GetBlockHash()) {
            gmListsCache.emplace(snapshot.GetBlockHash(), snapshot);
        } else {
            // !TODO: keep snapshots for yet alive quorums
        }
    }

    return snapshot;
}

CDeterministicGMList CDeterministicGMManager::GetListAtChainTip()
{
    LOCK(cs);
    if (!tipIndex) {
        return {};
    }
    return GetListForBlock(tipIndex);
}

bool CDeterministicGMManager::IsDIP3Enforced(int nHeight) const
{
    return Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V6_0);
}

bool CDeterministicGMManager::IsDIP3Enforced() const
{
    int tipHeight = WITH_LOCK(cs, return tipIndex ? tipIndex->nHeight : -1;);
    return IsDIP3Enforced(tipHeight);
}

bool CDeterministicGMManager::LegacyGMObsolete(int nHeight) const
{
    return nHeight > sporkManager.GetSporkValue(SPORK_21_LEGACY_GMS_MAX_HEIGHT);
}

bool CDeterministicGMManager::LegacyGMObsolete() const
{
    int tipHeight = WITH_LOCK(cs, return tipIndex ? tipIndex->nHeight : -1;);
    return LegacyGMObsolete(tipHeight);
}

void CDeterministicGMManager::CleanupCache(int nHeight)
{
    AssertLockHeld(cs);

    std::vector<uint256> toDeleteLists;
    std::vector<uint256> toDeleteDiffs;
    for (const auto& p : gmListsCache) {
        if (p.second.GetHeight() + LIST_DIFFS_CACHE_SIZE < nHeight) {
            toDeleteLists.emplace_back(p.first);
            continue;
        }
        // !TODO: llmq cache cleanup
    }
    for (const auto& h : toDeleteLists) {
        gmListsCache.erase(h);
    }
    for (const auto& p : gmListDiffsCache) {
        if (p.second.nHeight + LIST_DIFFS_CACHE_SIZE < nHeight) {
            toDeleteDiffs.emplace_back(p.first);
        }
    }
    for (const auto& h : toDeleteDiffs) {
        gmListDiffsCache.erase(h);
    }
}

std::vector<CDeterministicGMCPtr> CDeterministicGMManager::GetAllQuorumMembers(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum)
{
    auto& params = Params().GetConsensus().llmqs.at(llmqType);
    auto allMns = GetListForBlock(pindexQuorum);
    auto modifier = ::SerializeHash(std::make_pair(static_cast<uint8_t>(llmqType), pindexQuorum->GetBlockHash()));
    return allMns.CalculateQuorum(params.size, modifier);
}


