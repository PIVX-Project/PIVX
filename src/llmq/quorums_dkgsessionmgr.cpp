// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_dkgsessionmgr.h"

#include "chainparams.h"
#include "llmq/quorums_blockprocessor.h"
#include "llmq/quorums_utils.h"
#include "spork.h"
#include "validation.h"

namespace llmq
{

std::unique_ptr<CDKGSessionManager> quorumDKGSessionManager{nullptr};

static const std::string DB_VVEC = "qdkg_V";
static const std::string DB_SKCONTRIB = "qdkg_S";

CDKGSessionManager::CDKGSessionManager(CEvoDB& _evoDb, CBLSWorker& _blsWorker) :
        evoDb(_evoDb),
        blsWorker(_blsWorker)
{
    for (const auto& qt : Params().GetConsensus().llmqs) {
        dkgSessionHandlers.emplace(std::piecewise_construct,
                std::forward_as_tuple(qt.first),
                std::forward_as_tuple(qt.second, evoDb, blsWorker, *this));
    }
}

void CDKGSessionManager::StartThreads()
{
    for (auto& it : dkgSessionHandlers) {
        it.second.StartThread();
    }
}

void CDKGSessionManager::StopThreads()
{
    for (auto& it : dkgSessionHandlers) {
        it.second.StopThread();
    }
}

void CDKGSessionManager::UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload)
{
    CleanupCache();

    if (fInitialDownload)
        return;
    if (!deterministicMNManager->IsDIP3Enforced(pindexNew->nHeight))
        return;

    LOCK(cs_main);

    for (auto& qt : dkgSessionHandlers) {
        qt.second.UpdatedBlockTip(pindexNew);
    }
}

bool CDKGSessionManager::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (vRecv.empty()) {
        return false;
    }

    // peek into the message and see which LLMQType it is. First byte of all messages is always the LLMQType
    Consensus::LLMQType llmqType = (Consensus::LLMQType)*vRecv.begin();
    if (!dkgSessionHandlers.count(llmqType)) {
        return false;
    }

    dkgSessionHandlers.at(llmqType).ProcessMessage(pfrom, strCommand, vRecv);
    return true;
}

bool CDKGSessionManager::AlreadyHave(const CInv& inv) const
{
    for (const auto& p : dkgSessionHandlers) {
        auto& dkgType = p.second;
        if (dkgType.pendingContributions.HasSeen(inv.hash)
            || dkgType.pendingComplaints.HasSeen(inv.hash)
            || dkgType.pendingJustifications.HasSeen(inv.hash)
            || dkgType.pendingPrematureCommitments.HasSeen(inv.hash)) {
            return true;
        }
    }
    return false;
}

bool CDKGSessionManager::GetContribution(const uint256& hash, CDKGContribution& ret) const
{
    for (const auto& p : dkgSessionHandlers) {
        auto& dkgType = p.second;
        LOCK2(dkgType.cs, dkgType.curSession->invCs);
        if (dkgType.phase < QuorumPhase_Initialized || dkgType.phase > QuorumPhase_Contribute) {
            continue;
        }
        auto it = dkgType.curSession->contributions.find(hash);
        if (it != dkgType.curSession->contributions.end()) {
            ret = it->second;
            return true;
        }
    }
    return false;
}

bool CDKGSessionManager::GetComplaint(const uint256& hash, CDKGComplaint& ret) const
{
    for (const auto& p : dkgSessionHandlers) {
        auto& dkgType = p.second;
        LOCK2(dkgType.cs, dkgType.curSession->invCs);
        if (dkgType.phase < QuorumPhase_Contribute || dkgType.phase > QuorumPhase_Complain) {
            continue;
        }
        auto it = dkgType.curSession->complaints.find(hash);
        if (it != dkgType.curSession->complaints.end()) {
            ret = it->second;
            return true;
        }
    }
    return false;
}

bool CDKGSessionManager::GetJustification(const uint256& hash, CDKGJustification& ret) const
{
    for (const auto& p : dkgSessionHandlers) {
        auto& dkgType = p.second;
        LOCK2(dkgType.cs, dkgType.curSession->invCs);
        if (dkgType.phase < QuorumPhase_Complain || dkgType.phase > QuorumPhase_Justify) {
            continue;
        }
        auto it = dkgType.curSession->justifications.find(hash);
        if (it != dkgType.curSession->justifications.end()) {
            ret = it->second;
            return true;
        }
    }
    return false;
}

bool CDKGSessionManager::GetPrematureCommitment(const uint256& hash, CDKGPrematureCommitment& ret) const
{
    for (const auto& p : dkgSessionHandlers) {
        auto& dkgType = p.second;
        LOCK2(dkgType.cs, dkgType.curSession->invCs);
        if (dkgType.phase < QuorumPhase_Justify || dkgType.phase > QuorumPhase_Commit) {
            continue;
        }
        auto it = dkgType.curSession->prematureCommitments.find(hash);
        if (it != dkgType.curSession->prematureCommitments.end() && dkgType.curSession->validCommitments.count(hash)) {
            ret = it->second;
            return true;
        }
    }
    return false;
}

void CDKGSessionManager::WriteVerifiedVvecContribution(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& proTxHash, const BLSVerificationVectorPtr& vvec)
{
    evoDb.GetRawDB().Write(std::make_tuple(DB_VVEC, (uint8_t)llmqType, pindexQuorum->GetBlockHash(), proTxHash), *vvec);
}

void CDKGSessionManager::WriteVerifiedSkContribution(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& proTxHash, const CBLSSecretKey& skContribution)
{
    evoDb.GetRawDB().Write(std::make_tuple(DB_SKCONTRIB, (uint8_t)llmqType, pindexQuorum->GetBlockHash(), proTxHash), skContribution);
}

bool CDKGSessionManager::GetVerifiedContributions(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const std::vector<bool>& validMembers, std::vector<uint16_t>& memberIndexesRet, std::vector<BLSVerificationVectorPtr>& vvecsRet, BLSSecretKeyVector& skContributionsRet)
{
    auto members = deterministicMNManager->GetAllQuorumMembers(llmqType, pindexQuorum);

    memberIndexesRet.clear();
    vvecsRet.clear();
    skContributionsRet.clear();
    memberIndexesRet.reserve(members.size());
    vvecsRet.reserve(members.size());
    skContributionsRet.reserve(members.size());
    for (size_t i = 0; i < members.size(); i++) {
        if (validMembers[i]) {
            BLSVerificationVectorPtr vvec;
            CBLSSecretKey skContribution;
            if (!GetVerifiedContribution(llmqType, pindexQuorum, members[i]->proTxHash, vvec, skContribution)) {
                return false;
            }

            memberIndexesRet.emplace_back(i);
            vvecsRet.emplace_back(vvec);
            skContributionsRet.emplace_back(skContribution);
        }
    }
    return true;
}

bool CDKGSessionManager::GetVerifiedContribution(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& proTxHash, BLSVerificationVectorPtr& vvecRet, CBLSSecretKey& skContributionRet)
{
    const uint256& quorumHash = pindexQuorum->GetBlockHash();
    LOCK(contributionsCacheCs);
    ContributionsCacheKey cacheKey = {llmqType, quorumHash, proTxHash};
    auto it = contributionsCache.find(cacheKey);
    if (it != contributionsCache.end()) {
        vvecRet = it->second.vvec;
        skContributionRet = it->second.skContribution;
        return true;
    }

    BLSVerificationVector vvec;
    BLSVerificationVectorPtr vvecPtr;
    CBLSSecretKey skContribution;
    if (evoDb.GetRawDB().Read(std::make_tuple(DB_VVEC, (uint8_t)llmqType, quorumHash, proTxHash), vvec)) {
        vvecPtr = std::make_shared<BLSVerificationVector>(std::move(vvec));
    }
    evoDb.GetRawDB().Read(std::make_tuple(DB_SKCONTRIB, (uint8_t)llmqType, quorumHash, proTxHash), skContribution);

    it = contributionsCache.emplace(cacheKey, ContributionsCacheEntry{GetTimeMillis(), vvecPtr, skContribution}).first;

    vvecRet = it->second.vvec;
    skContributionRet = it->second.skContribution;

    return true;
}

void CDKGSessionManager::CleanupCache()
{
    LOCK(contributionsCacheCs);
    auto curTime = GetTimeMillis();
    for (auto it = contributionsCache.begin(); it != contributionsCache.end(); ) {
        if (curTime - it->second.entryTime > MAX_CONTRIBUTION_CACHE_TIME) {
            it = contributionsCache.erase(it);
        } else {
            ++it;
        }
    }
}

}
