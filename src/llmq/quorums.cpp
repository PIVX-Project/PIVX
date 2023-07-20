// Copyright (c) 2018 The Dash Core developers
// Copyright (c) 2023 The hemis developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "quorums.h"

#include "activemasternode.h"
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
    hw << q.pindexQuorum->GetBlockHash();
    for (const auto& dmn : q.members) {
        hw << dmn->proTxHash;
    }
    return hw.GetHash();
}

CQuorum::~CQuorum()
{
    // most likely the thread is already done
    stopCachePopulatorThread = true;
    if (cachePopulatorThread.joinable()) {
        cachePopulatorThread.join();
    }
}

void CQuorum::Init(const uint256& _minedBlockHash, const CBlockIndex* _pindexQuorum, const std::vector<CDeterministicMNCPtr>& _members, const std::vector<bool>& _validMembers, const CBLSPublicKey& _quorumPublicKey)
{
    minedBlockHash = _minedBlockHash;
    pindexQuorum = _pindexQuorum;
    members = _members;
    validMembers = _validMembers;
    quorumPublicKey = _quorumPublicKey;
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
            return validMembers[i];
        }
    }
    return false;
}

CBLSPublicKey CQuorum::GetPubKeyShare(size_t memberIdx) const
{
    if (quorumVvec == nullptr || memberIdx >= members.size() || !validMembers[memberIdx]) {
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
    LogPrintf("CQuorum::StartCachePopulatorThread -- start\n");

    // this thread will exit after some time
    // when then later some other thread tries to get keys, it will be much faster
    _this->cachePopulatorThread = std::thread(&TraceThread<std::function<void()> >, "quorum-cachepop", [_this, t] {
        for (size_t i = 0; i < _this->members.size() && !_this->stopCachePopulatorThread && !ShutdownRequested(); i++) {
            if (_this->validMembers[i]) {
                _this->GetPubKeyShare(i);
            }
        }
        LogPrintf("CQuorum::StartCachePopulatorThread -- done. time=%d\n", t.count());
    });
}

CQuorumManager::CQuorumManager(CEvoDB& _evoDb, CBLSWorker& _blsWorker, CDKGSessionManager& _dkgManager) : evoDb(_evoDb),
                                                                                                          blsWorker(_blsWorker),
                                                                                                          dkgManager(_dkgManager)
{
}

void CQuorumManager::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (fInitialDownload || !activeMasternodeManager || !deterministicMNManager->IsDIP3Enforced(pindexNew->nHeight)) {
        return;
    }

    LOCK(cs_main);

    for (auto& p : Params().GetConsensus().llmqs) {
        const auto& params = Params().GetConsensus().llmqs.at(p.first);

        auto lastQuorums = ScanQuorums(p.first, (size_t)params.keepOldConnections);

        llmq::EnsureLatestQuorumConnections(p.first, pindexNew, activeMasternodeManager->GetProTx(), lastQuorums);
    }
}


bool CQuorumManager::BuildQuorumFromCommitment(const CFinalCommitment& qc, const CBlockIndex* pindexQuorum, const uint256& minedBlockHash, std::shared_ptr<CQuorum>& quorum) const
{
    AssertLockHeld(cs_main);

    if (!mapBlockIndex.count(qc.quorumHash)) {
        LogPrintf("CQuorumManager::%s -- block %s not found", __func__, qc.quorumHash.ToString());
        return false;
    }
    auto& params = Params().GetConsensus().llmqs.at((Consensus::LLMQType)qc.llmqType);
    auto quorumIndex = mapBlockIndex[qc.quorumHash];
    auto members = deterministicMNManager->GetAllQuorumMembers((Consensus::LLMQType)qc.llmqType, pindexQuorum);
    quorum->Init(minedBlockHash, pindexQuorum, members, qc.validMembers, qc.quorumPublicKey);

    bool hasValidVvec = false;
    if (quorum->ReadContributions(evoDb)) {
        hasValidVvec = true;
    } else {
        if (BuildQuorumContributions(qc, quorum)) {
            quorum->WriteContributions(evoDb);
            hasValidVvec = true;
        } else {
            LogPrintf("CQuorumManager::%s -- quorum.ReadContributions and BuildQuorumContributions for block %s failed", __func__, qc.quorumHash.ToString());
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
        LogPrintf("CQuorumManager::%s -- failed to build quorumVvec\n", __func__);
        // without the quorum vvec, there can't be a skShare, so we fail here. Failure is not fatal here, as it still
        // allows to use the quorum as a non-member (verification through the quorum pub key)
        return false;
    }
    skShare = blsWorker.AggregateSecretKeys(skContributions);
    if (!skShare.IsValid()) {
        LogPrintf("CQuorumManager::%s -- failed to build skShare\n", __func__);
        // We don't bail out here as this is not a fatal error and still allows us to recover public key shares (as we
        // have a valid quorum vvec at this point)
    }
    t2.stop();

    LogPrintf("CQuorumManager::%s -- built quorum vvec and skShare. time=%d\n", __func__, t2.count());

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
    LOCK(cs_main);
    return ScanQuorums(llmqType, chainActive.Tip()->GetBlockHash(), maxCount);
}

std::vector<CQuorumCPtr> CQuorumManager::ScanQuorums(Consensus::LLMQType llmqType, const uint256& startBlock, size_t maxCount)
{
    std::vector<CQuorumCPtr> result;

    LOCK(cs_main);
    if (!mapBlockIndex.count(startBlock)) {
        return result;
    }

    result.reserve(maxCount);

    CBlockIndex* pindex = mapBlockIndex[startBlock];

    while (pindex != NULL && result.size() < maxCount && deterministicMNManager->IsDIP3Enforced(pindex->nHeight)) {
        if (HasQuorum(llmqType, pindex->GetBlockHash())) {
            auto quorum = GetQuorum(llmqType, pindex->GetBlockHash());
            if (quorum) {
                result.emplace_back(quorum);
            }
        }

        // TODO speedup (skip blocks where no quorums could have been mined)
        pindex = pindex->pprev;
    }

    return result;
}

CQuorumCPtr CQuorumManager::GetQuorum(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    CBlockIndex* pindexQuorum;
    {
        LOCK(cs_main);
        auto quorumIt = mapBlockIndex.find(quorumHash);

        if (quorumIt == mapBlockIndex.end()) {
            LogPrint(BCLog::LLMQ, "CQuorumManager::%s -- block %s not found", __func__, quorumHash.ToString());
            return nullptr;
        }
        pindexQuorum = quorumIt->second;
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

    auto it = quorumsCache.find(std::make_pair(llmqType, quorumHash));
    if (it != quorumsCache.end()) {
        return it->second;
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

    quorumsCache.emplace(std::make_pair(llmqType, quorumHash), quorum);

    return quorum;
}

CQuorumCPtr CQuorumManager::GetNewestQuorum(Consensus::LLMQType llmqType)
{
    auto quorums = ScanQuorums(llmqType, 1);
    if (quorums.empty()) {
        return nullptr;
    }
    return quorums.front();
}

} // namespace llmq
