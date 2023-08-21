// Copyright (c) 2018-2019 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_connections.h"

#include "evo/deterministicgms.h"
#include "llmq/quorums.h"
#include "net.h"
#include "tiertwo/gamemaster_meta_manager.h" // for g_mmetaman
#include "tiertwo/net_gamemasters.h"
#include "validation.h"

#include <vector>

namespace llmq
{


uint256 DeterministicOutboundConnection(const uint256& proTxHash1, const uint256& proTxHash2)
{
    // We need to deterministically select who is going to initiate the connection. The naive way would be to simply
    // return the min(proTxHash1, proTxHash2), but this would create a bias towards GMs with a numerically low
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

std::set<uint256> GetQuorumRelayMembers(const std::vector<CDeterministicGMCPtr>& gmList,
                                        unsigned int forMemberIndex)
{
    assert(forMemberIndex < gmList.size());

    // Special case
    if (gmList.size() == 2) {
        return {gmList[1 - forMemberIndex]->proTxHash};
    }

    // Relay to nodes at indexes (i+2^k)%n, where
    //   k: 0..max(1, floor(log2(n-1))-1)
    //   n: size of the quorum/ring
    std::set<uint256> r;
    int gap = 1;
    int gap_max = (int)gmList.size() - 1;
    int k = 0;
    while ((gap_max >>= 1) || k <= 1) {
        size_t idx = (forMemberIndex + gap) % gmList.size();
        r.emplace(gmList[idx]->proTxHash);
        gap <<= 1;
        k++;
    }
    return r;
}

static std::set<uint256> GetQuorumConnections(const std::vector<CDeterministicGMCPtr>& gms, const uint256& forMember, bool onlyOutbound)
{
    std::set<uint256> result;
    for (auto& dgm : gms) {
        if (dgm->proTxHash == forMember) {
            continue;
        }
        // Determine which of the two GMs (forMember vs dgm) should initiate the outbound connection and which
        // one should wait for the inbound connection. We do this in a deterministic way, so that even when we
        // end up with both connecting to each other, we know which one to disconnect
        uint256 deterministicOutbound = DeterministicOutboundConnection(forMember, dgm->proTxHash);
        if (!onlyOutbound || deterministicOutbound == dgm->proTxHash) {
            result.emplace(dgm->proTxHash);
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
        LogPrintf("CQuorumManager::%s -- removing gamemasters quorum connections for quorum %s:\n", __func__, qh.ToString());
        connman->removeQuorumNodes(llmqType, qh);
    }
}

// ensure connection to a given quorum
void EnsureQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash)
{
    const auto& members = deterministicGMManager->GetAllQuorumMembers(llmqType, pindexQuorum);
    auto itMember = std::find_if(members.begin(), members.end(), [&](const CDeterministicGMCPtr& dgm) { return dgm->proTxHash == myProTxHash; });
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
            auto gmList = deterministicGMManager->GetListAtChainTip();
            std::string debugMsg = strprintf("CLLMQUtils::%s -- adding gamemasters quorum connections for quorum %s:\n", __func__, pindexQuorum->GetBlockHash().ToString());
            for (auto& c : connections) {
                auto dgm = gmList.GetValidGM(c);
                if (!dgm) {
                    debugMsg += strprintf("  %s (not in valid GM set anymore)\n", c.ToString());
                } else {
                    debugMsg += strprintf("  %s (%s)\n", c.ToString(), dgm->pdgmState->addr.ToString());
                }
            }
            LogPrint(BCLog::LLMQ, debugMsg.c_str()); /* Continued */
        }
        connman->setQuorumNodes(llmqType, pindexQuorum->GetBlockHash(), connections);
    }
    if (!relayMembers.empty()) {
        auto connman = g_connman->GetTierTwoConnMan();
        connman->setGamemasterQuorumRelayMembers(llmqType, pindexQuorum->GetBlockHash(), relayMembers);
    }
}

void AddQuorumProbeConnections(Consensus::LLMQType llmqType, const CBlockIndex *pindexQuorum, const uint256 &myProTxHash)
{
    auto members = deterministicGMManager->GetAllQuorumMembers(llmqType, pindexQuorum);
    auto curTime = GetAdjustedTime();

    std::set<uint256> probeConnections;
    for (auto& dgm : members) {
        if (dgm->proTxHash == myProTxHash) {
            continue;
        }
        auto lastOutbound = g_mmetaman.GetMetaInfo(dgm->proTxHash)->GetLastOutboundSuccess();
        // re-probe after 50 minutes so that the "good connection" check in the DKG doesn't fail just because we're on
        // the brink of timeout
        if (curTime - lastOutbound > 50 * 60) {
            probeConnections.emplace(dgm->proTxHash);
        }
    }

    if (!probeConnections.empty()) {
        if (LogAcceptCategory(BCLog::LLMQ)) {
            auto gmList = deterministicGMManager->GetListAtChainTip();
            std::string debugMsg = strprintf("CLLMQUtils::%s -- adding gamemasters probes for quorum %s:\n", __func__, pindexQuorum->GetBlockHash().ToString());
            for (auto& c : probeConnections) {
                auto dgm = gmList.GetValidGM(c);
                if (!dgm) {
                    debugMsg += strprintf("  %s (not in valid GM set anymore)\n", c.ToString());
                } else {
                    debugMsg += strprintf("  %s (%s)\n", c.ToString(), dgm->pdgmState->addr.ToString());
                }
            }
            LogPrint(BCLog::LLMQ, debugMsg.c_str()); /* Continued */
        }
        g_connman->GetTierTwoConnMan()->addPendingProbeConnections(probeConnections);
    }
}

} // namespace llmq
