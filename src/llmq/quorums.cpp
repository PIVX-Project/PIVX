// Copyright (c) 2018 The Dash Core developers
// Copyright (c) 2023 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "quorums.h"

#include "activemasternode.h"
#include "chain.h"
#include "chainparams.h"
#include "cxxtimer.h"
#include "evo/deterministicmns.h"
#include "llmq/quorums_connections.h"
#include "logging.h"
#include "quorums_blockprocessor.h"
#include "quorums_commitment.h"
#include "quorums_dkgsessionmgr.h"
#include "shutdown.h"
#include "univalue.h"
#include "validation.h"

#include <cstddef>
#include <iostream>

namespace llmq
{

static const std::string DB_QUORUM_SK_SHARE = "q_Qsk";
static const std::string DB_QUORUM_QUORUM_VVEC = "q_Qqvvec";

std::unique_ptr<CQuorumManager> quorumManager{nullptr};

static uint256 MakeQuorumKey(const CQuorum& q)
{
    CHashWriter hw(SER_NETWORK, 0);
    hw << (uint8_t)q.params.type;
    hw << q.qc.quorumHash;
    for (const auto& dmn : q.members) {
        hw << dmn->proTxHash;
    }
    return hw.GetHash();
}

CQuorum::~CQuorum()
{
    // most likely the thread is already done
    stopCachePopulatorThread = true;
    // watch out to not join the thread when we're called from inside the thread, which might happen on shutdown. This
    // is because on shutdown the thread is the last owner of the shared CQuorum instance and thus the destroyer of it.
    if (cachePopulatorThread.joinable() && cachePopulatorThread.get_id() != std::this_thread::get_id()) {
        cachePopulatorThread.join();
    }
}

void CQuorum::Init(const CFinalCommitment& _qc, const CBlockIndex* _pindexQuorum, int _height, const uint256& _minedBlockHash, const std::vector<CDeterministicMNCPtr>& _members)
{
    qc = _qc;
    height = _height;
    pindexQuorum = _pindexQuorum;
    members = _members;
    minedBlockHash = _minedBlockHash;
}

bool CQuorum::IsMember(const uint256& proTxHash) const
{
    for (auto& dmn : members) {
        if (dmn->proTxHash == proTxHash) {
            return true;
        }
    }
    return false;
}

bool CQuorum::IsValidMember(const uint256& proTxHash) const
{
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i]->proTxHash == proTxHash) {
            return qc.validMembers[i];
        }
    }
    return false;
}

CBLSPublicKey CQuorum::GetPubKeyShare(size_t memberIdx) const
{
    if (quorumVvec == nullptr || memberIdx >= members.size() || !qc.validMembers[memberIdx]) {
        return CBLSPublicKey();
    }
    auto& m = members[memberIdx];
    return blsCache.BuildPubKeyShare(m->proTxHash, quorumVvec, CBLSId(m->proTxHash));
}

CBLSSecretKey CQuorum::GetSkShare() const
{
    return skShare;
}

int CQuorum::GetMemberIndex(const uint256& proTxHash) const
{
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i]->proTxHash == proTxHash) {
            return (int)i;
        }
    }
    return -1;
}

void CQuorum::WriteContributions(CEvoDB& evoDb)
{
    uint256 dbKey = MakeQuorumKey(*this);

    if (quorumVvec != nullptr) {
        evoDb.GetRawDB().Write(std::make_pair(DB_QUORUM_QUORUM_VVEC, dbKey), *quorumVvec);
    }
    if (skShare.IsValid()) {
        evoDb.GetRawDB().Write(std::make_pair(DB_QUORUM_SK_SHARE, dbKey), skShare);
    }
}

bool CQuorum::ReadContributions(CEvoDB& evoDb)
{
    uint256 dbKey = MakeQuorumKey(*this);

    BLSVerificationVector qv;
    if (evoDb.Read(std::make_pair(DB_QUORUM_QUORUM_VVEC, dbKey), qv)) {
        quorumVvec = std::make_shared<BLSVerificationVector>(std::move(qv));
    } else {
        return false;
    }

    // We ignore the return value here as it is ok if this fails. If it fails, it usually means that we are not a
    // member of the quorum but observed the whole DKG process to have the quorum verification vector.
    evoDb.Read(std::make_pair(DB_QUORUM_SK_SHARE, dbKey), skShare);

    return true;
}

void CQuorum::StartCachePopulatorThread(std::shared_ptr<CQuorum> _this)
{
    if (_this->quorumVvec == nullptr) {
        return;
    }

    cxxtimer::Timer t(true);
    LogPrint(BCLog::LLMQ, "CQuorum::StartCachePopulatorThread -- start\n");

    // this thread will exit after some time
    // when then later some other thread tries to get keys, it will be much faster
    _this->cachePopulatorThread = std::thread(&TraceThread<std::function<void()> >, "quorum-cachepop", [_this, t] {
        for (size_t i = 0; i < _this->members.size() && !_this->stopCachePopulatorThread && !ShutdownRequested(); i++) {
            if (_this->qc.validMembers[i]) {
                _this->GetPubKeyShare(i);
            }
        }
        LogPrint(BCLog::LLMQ, "CQuorum::StartCachePopulatorThread -- done. time=%d\n", t.count());
    });
}

CQuorumManager::CQuorumManager(CEvoDB& _evoDb, CBLSWorker& _blsWorker, CDKGSessionManager& _dkgManager) : evoDb(_evoDb),
                                                                                                          blsWorker(_blsWorker),
                                                                                                          dkgManager(_dkgManager)
{
    utils::InitQuorumsCache(mapQuorumsCache);
    utils::InitQuorumsCache(scanQuorumsCache);
}

void CQuorumManager::UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload)
{
    if (fInitialDownload || !activeMasternodeManager || !deterministicMNManager->IsDIP3Enforced(pindexNew->nHeight)) {
        return;
    }

    for (auto& p : Params().GetConsensus().llmqs) {
        const auto& params = Params().GetConsensus().llmqs.at(p.first);

        auto lastQuorums = ScanQuorums(p.first, pindexNew, (size_t)params.keepOldConnections);

        llmq::EnsureLatestQuorumConnections(p.first, pindexNew, activeMasternodeManager->GetProTx(), lastQuorums);
    }
}

bool CQuorumManager::BuildQuorumFromCommitment(const CFinalCommitment& qc, const CBlockIndex* pindexQuorum, const uint256& minedBlockHash, std::shared_ptr<CQuorum>& quorum) const
{
    assert(pindexQuorum);
    assert(qc.quorumHash == pindexQuorum->GetBlockHash());

    auto members = deterministicMNManager->GetAllQuorumMembers((Consensus::LLMQType)qc.llmqType, pindexQuorum);
    quorum->Init(qc, pindexQuorum, pindexQuorum->nHeight, minedBlockHash, members);

    bool hasValidVvec = false;
    if (quorum->ReadContributions(evoDb)) {
        hasValidVvec = true;
    } else {
        if (BuildQuorumContributions(qc, quorum)) {
            quorum->WriteContributions(evoDb);
            hasValidVvec = true;
        } else {
            LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- quorum.ReadContributions and BuildQuorumContributions for block %s failed\n", __func__, qc.quorumHash.ToString());
        }
    }

    if (hasValidVvec) {
        // pre-populate caches in the background
        // recovering public key shares is quite expensive and would result in serious lags for the first few signing
        // sessions if the shares would be calculated on-demand
        CQuorum::StartCachePopulatorThread(quorum);
    }

    return true;
}

bool CQuorumManager::BuildQuorumContributions(const CFinalCommitment& fqc, std::shared_ptr<CQuorum>& quorum) const
{
    std::vector<uint16_t> memberIndexes;
    std::vector<BLSVerificationVectorPtr> vvecs;
    BLSSecretKeyVector skContributions;
    if (!dkgManager.GetVerifiedContributions((Consensus::LLMQType)fqc.llmqType, quorum->pindexQuorum, fqc.validMembers, memberIndexes, vvecs, skContributions)) {
        return false;
    }

    BLSVerificationVectorPtr quorumVvec;
    CBLSSecretKey skShare;

    cxxtimer::Timer t2(true);
    quorumVvec = blsWorker.BuildQuorumVerificationVector(vvecs);
    if (quorumVvec == nullptr) {
        LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- failed to build quorumVvec\n", __func__);
        // without the quorum vvec, there can't be a skShare, so we fail here. Failure is not fatal here, as it still
        // allows to use the quorum as a non-member (verification through the quorum pub key)
        return false;
    }
    skShare = blsWorker.AggregateSecretKeys(skContributions);
    if (!skShare.IsValid()) {
        LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- failed to build skShare\n", __func__);
        // We don't bail out here as this is not a fatal error and still allows us to recover public key shares (as we
        // have a valid quorum vvec at this point)
    }
    t2.stop();

    LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- built quorum vvec and skShare. time=%d\n", __func__, t2.count());

    quorum->quorumVvec = quorumVvec;
    quorum->skShare = skShare;

    return true;
}

bool CQuorumManager::HasQuorum(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    return quorumBlockProcessor->HasMinedCommitment(llmqType, quorumHash);
}

std::vector<CQuorumCPtr> CQuorumManager::ScanQuorums(Consensus::LLMQType llmqType, size_t maxCount)
{
    const CBlockIndex* pindex = WITH_LOCK(cs_main, return chainActive.Tip());
    return ScanQuorums(llmqType, pindex, maxCount);
}

std::vector<CQuorumCPtr> CQuorumManager::ScanQuorums(Consensus::LLMQType llmqType, const CBlockIndex* pindexStart, size_t maxCount)
{
    if (pindexStart == nullptr || maxCount == 0) {
        return {};
    }

    bool fCacheExists{false};
    void* pIndexScanCommitments{(void*)pindexStart};
    size_t nScanCommitments{maxCount};
    std::vector<CQuorumCPtr> vecResultQuorums;

    {
        LOCK(quorumsCacheCs);
        auto& cache = scanQuorumsCache.at(llmqType);
        fCacheExists = cache.get(pindexStart->GetBlockHash(), vecResultQuorums);
        if (fCacheExists) {
            // We have exactly what requested so just return it
            if (vecResultQuorums.size() == maxCount) {
                return vecResultQuorums;
            }
            // If we have more cached than requested return only a subvector
            if (vecResultQuorums.size() > maxCount) {
                return {vecResultQuorums.begin(), vecResultQuorums.begin() + maxCount};
            }
            // If we have cached quorums but not enough, subtract what we have from the count and the set correct index where to start
            // scanning for the rests
            if (vecResultQuorums.size() > 0) {
                nScanCommitments -= vecResultQuorums.size();
                pIndexScanCommitments = (void*)vecResultQuorums.back()->pindexQuorum->pprev;
            }
        } else {
            // If there is nothing in cache request at least cache.max_size() because this gets cached then later
            nScanCommitments = std::max(maxCount, cache.max_size());
        }
    }
    // Get the block indexes of the mined commitments to build the required quorums from
    auto quorumIndexes = quorumBlockProcessor->GetMinedCommitmentsUntilBlock(llmqType, (const CBlockIndex*)pIndexScanCommitments, nScanCommitments);
    vecResultQuorums.reserve(vecResultQuorums.size() + quorumIndexes.size());

    for (auto& quorumIndex : quorumIndexes) {
        assert(quorumIndex);
        auto quorum = GetQuorum(llmqType, quorumIndex);
        assert(quorum != nullptr);
        vecResultQuorums.emplace_back(quorum);
    }

    size_t nCountResult{vecResultQuorums.size()};
    if (nCountResult > 0 && !fCacheExists) {
        LOCK(quorumsCacheCs);
        // Don't cache more than cache.max_size() elements
        auto& cache = scanQuorumsCache.at(llmqType);
        size_t nCacheEndIndex = std::min(nCountResult, cache.max_size());
        cache.emplace(pindexStart->GetBlockHash(), {vecResultQuorums.begin(), vecResultQuorums.begin() + nCacheEndIndex});
    }
    // Don't return more than nCountRequested elements
    size_t nResultEndIndex = std::min(nCountResult, maxCount);
    return {vecResultQuorums.begin(), vecResultQuorums.begin() + nResultEndIndex};
}

CQuorumCPtr CQuorumManager::GetQuorum(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    CBlockIndex* pindexQuorum;
    {
        LOCK(cs_main);
        pindexQuorum = LookupBlockIndex(quorumHash);

        if (!pindexQuorum) {
            LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- block %s not found.\n", __func__, quorumHash.ToString());
            return nullptr;
        }
    }
    return GetQuorum(llmqType, pindexQuorum);
}

CQuorumCPtr CQuorumManager::GetQuorum(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum)
{
    assert(pindexQuorum);

    auto quorumHash = pindexQuorum->GetBlockHash();
    // we must check this before we look into the cache. Reorgs might have happened which would mean we might have
    // cached quorums which are not in the active chain anymore
    if (!HasQuorum(llmqType, quorumHash)) {
        return nullptr;
    }

    LOCK(quorumsCacheCs);
    CQuorumCPtr pQuorum;
    if (mapQuorumsCache.at(llmqType).get(quorumHash, pQuorum)) {
        return pQuorum;
    }

    CFinalCommitment qc;
    uint256 retMinedBlockHash;
    if (!quorumBlockProcessor->GetMinedCommitment(llmqType, quorumHash, qc, retMinedBlockHash)) {
        return nullptr;
    }

    auto& params = Params().GetConsensus().llmqs.at(llmqType);

    auto quorum = std::make_shared<CQuorum>(params, blsWorker);
    if (!BuildQuorumFromCommitment(qc, pindexQuorum, retMinedBlockHash, quorum)) {
        return nullptr;
    }

    mapQuorumsCache.at(llmqType).emplace(quorumHash, quorum);

    return quorum;
}

} // namespace llmq
