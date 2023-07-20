// Copyright (c) 2018-2019 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_connections.h"

#include "evo/deterministicmns.h"
#include "llmq/quorums.h"
#include "net.h"
#include "tiertwo/masternode_meta_manager.h" // for g_mmetaman
#include "tiertwo/net_masternodes.h"
#include "validation.h"

#include <vector>

namespace llmq
{


uint256 DeterministicOutboundConnection(const uint256& proTxHash1, const uint256& proTxHash2)
{
    // We need to deterministically select who is going to initiate the connection. The naive way would be to simply
    // return the min(proTxHash1, proTxHash2), but this would create a bias towards MNs with a numerically low
    // hash. To fix this, we return the proTxHash that has the lowest value of:
    //   hash(min(proTxHash1, proTxHash2), max(proTxHash1, proTxHash2), proTxHashX)
    // where proTxHashX is the proTxHash to compare
    uint256 h1;
    uint256 h2;
    if (proTxHash1 < proTxHash2) {
        h1 = ::SerializeHash(std::make_tuple(proTxHash1, proTxHash2, proTxHash1));
        h2 = ::SerializeHash(std::make_tuple(proTxHash1, proTxHash2, proTxHash2));
    } else {
        h1 = ::SerializeHash(std::make_tuple(proTxHash2, proTxHash1, proTxHash1));
        h2 = ::SerializeHash(std::make_tuple(proTxHash2, proTxHash1, proTxHash2));
    }
    if (h1 < h2) {
        return proTxHash1;
    }
    return proTxHash2;
}

std::set<uint256> GetQuorumRelayMembers(const std::vector<CDeterministicMNCPtr>& mnList,
                                        unsigned int forMemberIndex)
{
    assert(forMemberIndex < mnList.size());

    // Special case
    if (mnList.size() == 2) {
        return {mnList[1 - forMemberIndex]->proTxHash};
    }

    // Relay to nodes at indexes (i+2^k)%n, where
    //   k: 0..max(1, floor(log2(n-1))-1)
    //   n: size of the quorum/ring
    std::set<uint256> r;
    int gap = 1;
    int gap_max = (int)mnList.size() - 1;
    int k = 0;
    while ((gap_max >>= 1) || k <= 1) {
        size_t idx = (forMemberIndex + gap) % mnList.size();
        r.emplace(mnList[idx]->proTxHash);
        gap <<= 1;
        k++;
    }
    return r;
}

static std::set<uint256> GetQuorumConnections(const std::vector<CDeterministicMNCPtr>& mns, const uint256& forMember, bool onlyOutbound)
{
    std::set<uint256> result;
    for (auto& dmn : mns) {
        if (dmn->proTxHash == forMember) {
            continue;
        }
        // Determine which of the two MNs (forMember vs dmn) should initiate the outbound connection and which
        // one should wait for the inbound connection. We do this in a deterministic way, so that even when we
        // end up with both connecting to each other, we know which one to disconnect
        uint256 deterministicOutbound = DeterministicOutboundConnection(forMember, dmn->proTxHash);
        if (!onlyOutbound || deterministicOutbound == dmn->proTxHash) {
            result.emplace(dmn->proTxHash);
        }
    }
    return result;
}

std::set<size_t> CalcDeterministicWatchConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, size_t memberCount, size_t connectionCount)
{
    static uint256 qwatchConnectionSeed;
    static std::atomic<bool> qwatchConnectionSeedGenerated{false};
    static RecursiveMutex qwatchConnectionSeedCs;
    if (!qwatchConnectionSeedGenerated) {
        LOCK(qwatchConnectionSeedCs);
        if (!qwatchConnectionSeedGenerated) {
            qwatchConnectionSeed = GetRandHash();
            qwatchConnectionSeedGenerated = true;
        }
    }

    std::set<size_t> result;
    uint256 rnd = qwatchConnectionSeed;
    for (size_t i = 0; i < connectionCount; i++) {
        rnd = ::SerializeHash(std::make_pair(rnd, std::make_pair(static_cast<uint8_t>(llmqType), pindexQuorum->GetBlockHash())));
        result.emplace(rnd.GetUint64(0) % memberCount);
    }
    return result;
}

// ensure connection to a given list of quorums
void EnsureLatestQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexNew, const uint256& myProTxHash, std::vector<CQuorumCPtr>& lastQuorums)
{
    AssertLockHeld(cs_main);

    const auto& params = Params().GetConsensus().llmqs.at(llmqType);
    auto connman = g_connman->GetTierTwoConnMan();

    auto connmanQuorumsToDelete = connman->getQuorumNodes(llmqType);

    // don't remove connections for the currently in-progress DKG round
    int curDkgHeight = pindexNew->nHeight - (pindexNew->nHeight % params.dkgInterval);
    auto curDkgBlock = chainActive[curDkgHeight]->GetBlockHash();
    connmanQuorumsToDelete.erase(curDkgBlock);

    for (auto& quorum : lastQuorums) {
        if (!quorum->IsMember(myProTxHash)) {
            continue;
        }

        if (!connman->hasQuorumNodes(llmqType, quorum->pindexQuorum->GetBlockHash())) {
            EnsureQuorumConnections(llmqType, quorum->pindexQuorum, myProTxHash);
        }
        connmanQuorumsToDelete.erase(quorum->pindexQuorum->GetBlockHash());
    }

    for (auto& qh : connmanQuorumsToDelete) {
        LogPrintf("CQuorumManager::%s -- removing masternodes quorum connections for quorum %s:\n", __func__, qh.ToString());
        connman->removeQuorumNodes(llmqType, qh);
    }
}

// ensure connection to a given quorum
void EnsureQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash)
{
    const auto& members = deterministicMNManager->GetAllQuorumMembers(llmqType, pindexQuorum);
    auto itMember = std::find_if(members.begin(), members.end(), [&](const CDeterministicMNCPtr& dmn) { return dmn->proTxHash == myProTxHash; });
    bool isMember = itMember != members.end();

    if (!isMember) { // && !CLLMQUtils::IsWatchQuorumsEnabled()) {
        return;
    }

    std::set<uint256> connections;
    std::set<uint256> relayMembers;
    if (isMember) {
        connections = GetQuorumConnections(members, myProTxHash, true);
        unsigned int memberIndex = itMember - members.begin();
        relayMembers = GetQuorumRelayMembers(members, memberIndex);
    } else {
        auto cindexes = CalcDeterministicWatchConnections(llmqType, pindexQuorum, members.size(), 1);
        for (auto idx : cindexes) {
            connections.emplace(members[idx]->proTxHash);
        }
        relayMembers = connections;
    }
    if (!connections.empty()) {
        auto connman = g_connman->GetTierTwoConnMan();
        if (!connman->hasQuorumNodes(llmqType, pindexQuorum->GetBlockHash()) && LogAcceptCategory(BCLog::LLMQ)) {
            auto mnList = deterministicMNManager->GetListAtChainTip();
            std::string debugMsg = strprintf("CLLMQUtils::%s -- adding masternodes quorum connections for quorum %s:\n", __func__, pindexQuorum->GetBlockHash().ToString());
            for (auto& c : connections) {
                auto dmn = mnList.GetValidMN(c);
                if (!dmn) {
                    debugMsg += strprintf("  %s (not in valid MN set anymore)\n", c.ToString());
                } else {
                    debugMsg += strprintf("  %s (%s)\n", c.ToString(), dmn->pdmnState->addr.ToString());
                }
            }
            LogPrint(BCLog::LLMQ, debugMsg.c_str()); /* Continued */
        }
        connman->setQuorumNodes(llmqType, pindexQuorum->GetBlockHash(), connections);
    }
    if (!relayMembers.empty()) {
        auto connman = g_connman->GetTierTwoConnMan();
        connman->setMasternodeQuorumRelayMembers(llmqType, pindexQuorum->GetBlockHash(), relayMembers);
    }
}

void AddQuorumProbeConnections(Consensus::LLMQType llmqType, const CBlockIndex *pindexQuorum, const uint256 &myProTxHash)
{
    auto members = deterministicMNManager->GetAllQuorumMembers(llmqType, pindexQuorum);
    auto curTime = GetAdjustedTime();

    std::set<uint256> probeConnections;
    for (auto& dmn : members) {
        if (dmn->proTxHash == myProTxHash) {
            continue;
        }
        auto lastOutbound = g_mmetaman.GetMetaInfo(dmn->proTxHash)->GetLastOutboundSuccess();
        // re-probe after 50 minutes so that the "good connection" check in the DKG doesn't fail just because we're on
        // the brink of timeout
        if (curTime - lastOutbound > 50 * 60) {
            probeConnections.emplace(dmn->proTxHash);
        }
    }

    if (!probeConnections.empty()) {
        if (LogAcceptCategory(BCLog::LLMQ)) {
            auto mnList = deterministicMNManager->GetListAtChainTip();
            std::string debugMsg = strprintf("CLLMQUtils::%s -- adding masternodes probes for quorum %s:\n", __func__, pindexQuorum->GetBlockHash().ToString());
            for (auto& c : probeConnections) {
                auto dmn = mnList.GetValidMN(c);
                if (!dmn) {
                    debugMsg += strprintf("  %s (not in valid MN set anymore)\n", c.ToString());
                } else {
                    debugMsg += strprintf("  %s (%s)\n", c.ToString(), dmn->pdmnState->addr.ToString());
                }
            }
            LogPrint(BCLog::LLMQ, debugMsg.c_str()); /* Continued */
        }
        g_connman->GetTierTwoConnMan()->addPendingProbeConnections(probeConnections);
    }
}

} // namespace llmq
