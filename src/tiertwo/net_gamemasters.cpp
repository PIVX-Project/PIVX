// Copyright (c) 2020 The Dash developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/net_gamemasters.h"

#include "chainparams.h"
#include "evo/deterministicgms.h"
#include "llmq/quorums.h"
#include "netmessagemaker.h"
#include "scheduler.h"
#include "tiertwo/gamemaster_meta_manager.h" // for g_mmetaman
#include "tiertwo/tiertwo_sync_state.h"

TierTwoConnMan::TierTwoConnMan(CConnman* _connman) : connman(_connman) {}
TierTwoConnMan::~TierTwoConnMan() { connman = nullptr; }

void TierTwoConnMan::setQuorumNodes(Consensus::LLMQType llmqType,
                                    const uint256& quorumHash,
                                    const std::set<uint256>& proTxHashes)
{
    LOCK(cs_vPendingGamemasters);
    auto it = gamemasterQuorumNodes.emplace(QuorumTypeAndHash(llmqType, quorumHash), proTxHashes);
    if (!it.second) {
        it.first->second = proTxHashes;
    }
}

std::set<uint256> TierTwoConnMan::getQuorumNodes(Consensus::LLMQType llmqType)
{
    LOCK(cs_vPendingGamemasters);
    std::set<uint256> result;
    for (const auto& p : gamemasterQuorumNodes) {
        if (p.first.first != llmqType) {
            continue;
        }
        result.emplace(p.first.second);
    }
    return result;
}

std::set<NodeId> TierTwoConnMan::getQuorumNodes(Consensus::LLMQType llmqType, uint256 quorumHash)
{
    std::set<NodeId> result;
    auto it = WITH_LOCK(cs_vPendingGamemasters, return gamemasterQuorumRelayMembers.find(std::make_pair(llmqType, quorumHash)));
    if (WITH_LOCK(cs_vPendingGamemasters, return it == gamemasterQuorumRelayMembers.end())) {
        return {};
    }
    for (const auto pnode : connman->GetvNodes()) {
        if (pnode->fDisconnect) {
            continue;
        }
        if (!it->second.count(pnode->verifiedProRegTxHash)) {
            continue;
        }
        // is it a valid member?
        if (!llmq::quorumManager->GetQuorum(llmqType, quorumHash)) {
            continue;
        }
        if (!llmq::quorumManager->GetQuorum(llmqType, quorumHash)->IsValidMember(pnode->verifiedProRegTxHash)) {
            continue;
        }
        result.emplace(pnode->GetId());
    }
    return result;
}

bool TierTwoConnMan::hasQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    LOCK(cs_vPendingGamemasters);
    return gamemasterQuorumNodes.count(QuorumTypeAndHash(llmqType, quorumHash));
}

void TierTwoConnMan::removeQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    LOCK(cs_vPendingGamemasters);
    gamemasterQuorumNodes.erase(std::make_pair(llmqType, quorumHash));
}

void TierTwoConnMan::setGamemasterQuorumRelayMembers(Consensus::LLMQType llmqType, const uint256& quorumHash, const std::set<uint256>& proTxHashes)
{
    {
        LOCK(cs_vPendingGamemasters);
        auto it = gamemasterQuorumRelayMembers.emplace(std::make_pair(llmqType, quorumHash), proTxHashes);
        if (!it.second) {
            it.first->second = proTxHashes;
        }
    }

    // Update existing connections
    connman->ForEachNode([&](CNode* pnode) {
        if (!pnode->m_gamemaster_iqr_connection && isGamemasterQuorumRelayMember(pnode->verifiedProRegTxHash)) {
            // Tell our peer that we're interested in plain LLMQ recovered signatures.
            // Otherwise, the peer would only announce/send messages resulting from QRECSIG,
            // future e.g. tx locks or chainlocks. SPV and regular full nodes should not send
            // this message as they are usually only interested in the higher level messages.
            CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(NetMsgType::QSENDRECSIGS, true));
            pnode->m_gamemaster_iqr_connection = true;
        }
    });
}

bool TierTwoConnMan::isGamemasterQuorumNode(const CNode* pnode)
{
    // Let's see if this is an outgoing connection to an address that is known to be a gamemaster
    // We however only need to know this if the node did not authenticate itself as a GM yet
    uint256 assumedProTxHash;
    if (pnode->verifiedProRegTxHash.IsNull() && !pnode->fInbound) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        auto dgm = gmList.GetGMByService(pnode->addr);
        if (dgm == nullptr) {
            // This is definitely not a gamemaster
            return false;
        }
        assumedProTxHash = dgm->proTxHash;
    }

    LOCK(cs_vPendingGamemasters);
    for (const auto& quorumConn : gamemasterQuorumNodes) {
        if (!pnode->verifiedProRegTxHash.IsNull()) {
            if (quorumConn.second.count(pnode->verifiedProRegTxHash)) {
                return true;
            }
        } else if (!assumedProTxHash.IsNull()) {
            if (quorumConn.second.count(assumedProTxHash)) {
                return true;
            }
        }
    }
    return false;
}

bool TierTwoConnMan::isGamemasterQuorumRelayMember(const uint256& protxHash)
{
    if (protxHash.IsNull()) {
        return false;
    }
    LOCK(cs_vPendingGamemasters);
    for (const auto& p : gamemasterQuorumRelayMembers) {
        if (p.second.count(protxHash) > 0) {
            return true;
        }
    }
    return false;
}

bool TierTwoConnMan::addPendingGamemaster(const uint256& proTxHash)
{
    LOCK(cs_vPendingGamemasters);
    if (std::find(vPendingGamemasters.begin(), vPendingGamemasters.end(), proTxHash) != vPendingGamemasters.end()) {
        return false;
    }
    vPendingGamemasters.emplace_back(proTxHash);
    return true;
}

void TierTwoConnMan::addPendingProbeConnections(const std::set<uint256>& proTxHashes)
{
    LOCK(cs_vPendingGamemasters);
    gamemasterPendingProbes.insert(proTxHashes.begin(), proTxHashes.end());
}

void TierTwoConnMan::clear()
{
    LOCK(cs_vPendingGamemasters);
    gamemasterQuorumNodes.clear();
    gamemasterQuorumRelayMembers.clear();
    vPendingGamemasters.clear();
    gamemasterPendingProbes.clear();
}

void TierTwoConnMan::start(CScheduler& scheduler, const TierTwoConnMan::Options& options)
{
    // Must be started after connman
    assert(connman);
    interruptNet.reset();

    // Connecting to specific addresses, no gamemaster connections available
    if (options.m_has_specified_outgoing) return;
    // Initiate gamemaster connections
    threadOpenGamemasterConnections = std::thread(&TraceThread<std::function<void()> >, "gmcon", std::function<void()>(std::bind(&TierTwoConnMan::ThreadOpenGamemasterConnections, this)));
    // Cleanup process every 60 seconds
    scheduler.scheduleEvery(std::bind(&TierTwoConnMan::doMaintenance, this), 60 * 1000);
}

void TierTwoConnMan::stop() {
    if (threadOpenGamemasterConnections.joinable()) {
        threadOpenGamemasterConnections.join();
    }
}

void TierTwoConnMan::interrupt()
{
    interruptNet();
}

void TierTwoConnMan::openConnection(const CAddress& addrConnect, bool isProbe)
{
    if (interruptNet) return;
    // Note: using ip:port string connection instead of the addr to bypass the "only connect to single IPs" validation.
    std::string conn = addrConnect.ToStringIPPort();
    CAddress dummyAddr;
    connman->OpenNetworkConnection(dummyAddr, false, nullptr, conn.data(), false, false, false, true, isProbe);
}

class PeerData {
public:
    PeerData(const CService& s, bool disconnect, bool is_gm_conn) : service(s), f_disconnect(disconnect), f_is_gm_conn(is_gm_conn) {}
    const CService service;
    bool f_disconnect{false};
    bool f_is_gm_conn{false};
    bool operator==(const CService& s) const { return service == s; }
};

struct GmService {
public:
    uint256 verif_proreg_tx_hash{UINT256_ZERO};
    bool is_inbound{false};
    bool operator==(const uint256& hash) const { return verif_proreg_tx_hash == hash; }
};

void TierTwoConnMan::ThreadOpenGamemasterConnections()
{
    const auto& chainParams = Params();
    bool triedConnect = false;
    while (!interruptNet) {

        // Retry every 0.1 seconds if a connection was created, otherwise 1.5 seconds
        int sleepTime = triedConnect ? 100 : (chainParams.IsRegTestNet() ? 200 : 1500);
        if (!interruptNet.sleep_for(std::chrono::milliseconds(sleepTime))) {
            return;
        }

        triedConnect = false;

        if (!fMasterNode || !g_tiertwo_sync_state.IsBlockchainSynced() || !g_connman->GetNetworkActive()) {
            continue;
        }

        // Gather all connected peers first, so we don't
        // try to connect to an already connected peer
        std::vector<PeerData> connectedNodes;
        std::vector<GmService> connectedGmServices;
        connman->ForEachNode([&](const CNode* pnode) {
            connectedNodes.emplace_back(PeerData{pnode->addr, pnode->fDisconnect, pnode->m_gamemaster_connection});
            if (!pnode->verifiedProRegTxHash.IsNull()) {
                connectedGmServices.emplace_back(GmService{pnode->verifiedProRegTxHash, pnode->fInbound});
            }
        });

        // Try to connect to a single GM per cycle
        CDeterministicGMCPtr dgmToConnect{nullptr};
        // Current list
        auto gmList = deterministicGMManager->GetListAtChainTip();
        int64_t currentTime = GetAdjustedTime();
        bool isProbe = false;
        {
            LOCK(cs_vPendingGamemasters);

            // First try to connect to pending GMs
            if (!vPendingGamemasters.empty()) {
                auto dgm = gmList.GetValidGM(vPendingGamemasters.front());
                vPendingGamemasters.erase(vPendingGamemasters.begin());
                if (dgm) {
                    auto peerData = std::find(connectedNodes.begin(), connectedNodes.end(), dgm->pdgmState->addr);
                    if (peerData == std::end(connectedNodes)) {
                        dgmToConnect = dgm;
                        LogPrint(BCLog::NET_GM, "%s -- opening pending gamemaster connection to %s, service=%s\n",
                                 __func__, dgm->proTxHash.ToString(), dgm->pdgmState->addr.ToString());
                    }
                }
            }

            // Secondly, try to connect quorum members
            if (!dgmToConnect) {
                std::vector<CDeterministicGMCPtr> pending;
                for (const auto& group: gamemasterQuorumNodes) {
                    for (const auto& proRegTxHash: group.second) {
                        // Skip if already have this member connected
                        if (std::count(connectedGmServices.begin(), connectedGmServices.end(), proRegTxHash) > 0) {
                            continue;
                        }

                        // Don't try to connect to ourselves
                        if (WITH_LOCK(cs_vPendingGamemasters, return local_dgm_pro_tx_hash && *local_dgm_pro_tx_hash == proRegTxHash)) {
                            continue;
                        }

                        // Check if DGM exists in tip list
                        const auto& dgm = gmList.GetValidGM(proRegTxHash);
                        if (!dgm) continue;
                        auto peerData = std::find(connectedNodes.begin(), connectedNodes.end(), dgm->pdgmState->addr);

                        // Skip already connected nodes.
                        if (peerData != std::end(connectedNodes) &&
                            (peerData->f_disconnect || peerData->f_is_gm_conn)) {
                            continue;
                        }

                        // Check if we already tried this connection recently to not retry too often
                        int64_t lastAttempt = g_mmetaman.GetMetaInfo(dgm->proTxHash)->GetLastOutboundAttempt();
                        // back off trying connecting to an address if we already tried recently
                        if (currentTime - lastAttempt < chainParams.LLMQConnectionRetryTimeout()) {
                            continue;
                        }
                        pending.emplace_back(dgm);
                    }
                }
                // Select a random node to connect
                if (!pending.empty()) {
                    dgmToConnect = pending[GetRandInt((int) pending.size())];
                    LogPrint(BCLog::NET_GM, "TierTwoConnMan::%s -- opening quorum connection to %s, service=%s\n",
                             __func__, dgmToConnect->proTxHash.ToString(), dgmToConnect->pdgmState->addr.ToString());
                }
            }

            // If no node was selected, let's try to probe nodes connection
            if (!dgmToConnect) {
                std::vector<CDeterministicGMCPtr> pending;
                for (auto it = gamemasterPendingProbes.begin(); it != gamemasterPendingProbes.end(); ) {
                    auto dgm = gmList.GetGM(*it);
                    if (!dgm) {
                        it = gamemasterPendingProbes.erase(it);
                        continue;
                    }

                    // Discard already connected outbound GMs
                    auto gmService = std::find(connectedGmServices.begin(), connectedGmServices.end(), dgm->proTxHash);
                    bool connectedAndOutbound = gmService != std::end(connectedGmServices) && !gmService->is_inbound;
                    if (connectedAndOutbound) {
                        // we already have an outbound connection to this GM so there is no eed to probe it again
                        g_mmetaman.GetMetaInfo(dgm->proTxHash)->SetLastOutboundSuccess(currentTime);
                        it = gamemasterPendingProbes.erase(it);
                        continue;
                    }

                    ++it;

                    int64_t lastAttempt = g_mmetaman.GetMetaInfo(dgm->proTxHash)->GetLastOutboundAttempt();
                    // back off trying connecting to an address if we already tried recently
                    if (currentTime - lastAttempt < chainParams.LLMQConnectionRetryTimeout()) {
                        continue;
                    }
                    pending.emplace_back(dgm);
                }

                // Select a random node to connect
                if (!pending.empty()) {
                    dgmToConnect = pending[GetRandInt((int)pending.size())];
                    gamemasterPendingProbes.erase(dgmToConnect->proTxHash);
                    isProbe = true;

                    LogPrint(BCLog::NET_GM, "CConnman::%s -- probing gamemaster %s, service=%s\n",
                             __func__, dgmToConnect->proTxHash.ToString(), dgmToConnect->pdgmState->addr.ToString());
                }
            }
        }

        // No DGM to connect
        if (!dgmToConnect || interruptNet) {
            continue;
        }

        // Update last attempt and try connection
        g_mmetaman.GetMetaInfo(dgmToConnect->proTxHash)->SetLastOutboundAttempt(currentTime);
        triedConnect = true;

        // Now connect
        openConnection(CAddress(dgmToConnect->pdgmState->addr, NODE_NETWORK), isProbe);
        // should be in the list now if connection was opened
        bool connected = connman->ForNode(dgmToConnect->pdgmState->addr, CConnman::AllNodes, [&](CNode* pnode) {
            if (pnode->fDisconnect) { LogPrintf("about to be disconnected\n");
                return false;
            }
            return true;
        });
        if (!connected) {
            LogPrint(BCLog::NET_GM, "TierTwoConnMan::%s -- connection failed for gamemaster  %s, service=%s\n",
                     __func__, dgmToConnect->proTxHash.ToString(), dgmToConnect->pdgmState->addr.ToString());
            // reset last outbound success
            g_mmetaman.GetMetaInfo(dgmToConnect->proTxHash)->SetLastOutboundSuccess(0);
        }
    }
}

static void ProcessGamemasterConnections(CConnman& connman, TierTwoConnMan& tierTwoConnMan)
{
    // Don't disconnect gamemaster connections when we have less than the desired amount of outbound nodes
    int nonGamemasterCount = 0;
    connman.ForEachNode([&](CNode* pnode) {
        if (!pnode->fInbound && !pnode->fFeeler && !pnode->fAddnode && !pnode->m_gamemaster_connection && !pnode->m_gamemaster_probe_connection) {
            nonGamemasterCount++;
        }
    });
    if (nonGamemasterCount < (int) connman.GetMaxOutboundNodeCount()) {
        return;
    }

    connman.ForEachNode([&](CNode* pnode) {
        // we're only disconnecting m_gamemaster_connection connections
        if (!pnode->m_gamemaster_connection) return;
        // we're only disconnecting outbound connections (inbound connections are disconnected in AcceptConnection())
        if (pnode->fInbound) return;
        // we're not disconnecting LLMQ connections
        if (tierTwoConnMan.isGamemasterQuorumNode(pnode)) return;
        // we're not disconnecting gamemaster probes for at least a few seconds
        if (pnode->m_gamemaster_probe_connection && GetSystemTimeInSeconds() - pnode->nTimeConnected < 5) return;

        if (fLogIPs) {
            LogPrintf("Closing Gamemaster connection: peer=%d, addr=%s\n", pnode->GetId(), pnode->addr.ToString());
        } else {
            LogPrintf("Closing Gamemaster connection: peer=%d\n", pnode->GetId());
        }
        pnode->fDisconnect = true;
    });
}

void TierTwoConnMan::doMaintenance()
{
    if(!g_tiertwo_sync_state.IsBlockchainSynced() || interruptNet) {
        return;
    }
    ProcessGamemasterConnections(*connman, *this);
}

