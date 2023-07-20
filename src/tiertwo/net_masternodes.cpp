// Copyright (c) 2020 The Dash developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/net_masternodes.h"

#include "chainparams.h"
#include "evo/deterministicmns.h"
#include "llmq/quorums.h"
#include "netmessagemaker.h"
#include "scheduler.h"
#include "tiertwo/masternode_meta_manager.h" // for g_mmetaman
#include "tiertwo/tiertwo_sync_state.h"

TierTwoConnMan::TierTwoConnMan(CConnman* _connman) : connman(_connman) {}
TierTwoConnMan::~TierTwoConnMan() { connman = nullptr; }

void TierTwoConnMan::setQuorumNodes(Consensus::LLMQType llmqType,
                                    const uint256& quorumHash,
                                    const std::set<uint256>& proTxHashes)
{
    LOCK(cs_vPendingMasternodes);
    auto it = masternodeQuorumNodes.emplace(QuorumTypeAndHash(llmqType, quorumHash), proTxHashes);
    if (!it.second) {
        it.first->second = proTxHashes;
    }
}

std::set<uint256> TierTwoConnMan::getQuorumNodes(Consensus::LLMQType llmqType)
{
    LOCK(cs_vPendingMasternodes);
    std::set<uint256> result;
    for (const auto& p : masternodeQuorumNodes) {
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
    auto it = WITH_LOCK(cs_vPendingMasternodes, return masternodeQuorumRelayMembers.find(std::make_pair(llmqType, quorumHash)));
    if (WITH_LOCK(cs_vPendingMasternodes, return it == masternodeQuorumRelayMembers.end())) {
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
    LOCK(cs_vPendingMasternodes);
    return masternodeQuorumNodes.count(QuorumTypeAndHash(llmqType, quorumHash));
}

void TierTwoConnMan::removeQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    LOCK(cs_vPendingMasternodes);
    masternodeQuorumNodes.erase(std::make_pair(llmqType, quorumHash));
}

void TierTwoConnMan::setMasternodeQuorumRelayMembers(Consensus::LLMQType llmqType, const uint256& quorumHash, const std::set<uint256>& proTxHashes)
{
    {
        LOCK(cs_vPendingMasternodes);
        auto it = masternodeQuorumRelayMembers.emplace(std::make_pair(llmqType, quorumHash), proTxHashes);
        if (!it.second) {
            it.first->second = proTxHashes;
        }
    }

    // Update existing connections
    connman->ForEachNode([&](CNode* pnode) {
        if (!pnode->m_masternode_iqr_connection && isMasternodeQuorumRelayMember(pnode->verifiedProRegTxHash)) {
            // Tell our peer that we're interested in plain LLMQ recovered signatures.
            // Otherwise, the peer would only announce/send messages resulting from QRECSIG,
            // future e.g. tx locks or chainlocks. SPV and regular full nodes should not send
            // this message as they are usually only interested in the higher level messages.
            CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(NetMsgType::QSENDRECSIGS, true));
            pnode->m_masternode_iqr_connection = true;
        }
    });
}

bool TierTwoConnMan::isMasternodeQuorumNode(const CNode* pnode)
{
    // Let's see if this is an outgoing connection to an address that is known to be a masternode
    // We however only need to know this if the node did not authenticate itself as a MN yet
    uint256 assumedProTxHash;
    if (pnode->verifiedProRegTxHash.IsNull() && !pnode->fInbound) {
        auto mnList = deterministicMNManager->GetListAtChainTip();
        auto dmn = mnList.GetMNByService(pnode->addr);
        if (dmn == nullptr) {
            // This is definitely not a masternode
            return false;
        }
        assumedProTxHash = dmn->proTxHash;
    }

    LOCK(cs_vPendingMasternodes);
    for (const auto& quorumConn : masternodeQuorumNodes) {
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

bool TierTwoConnMan::isMasternodeQuorumRelayMember(const uint256& protxHash)
{
    if (protxHash.IsNull()) {
        return false;
    }
    LOCK(cs_vPendingMasternodes);
    for (const auto& p : masternodeQuorumRelayMembers) {
        if (p.second.count(protxHash) > 0) {
            return true;
        }
    }
    return false;
}

bool TierTwoConnMan::addPendingMasternode(const uint256& proTxHash)
{
    LOCK(cs_vPendingMasternodes);
    if (std::find(vPendingMasternodes.begin(), vPendingMasternodes.end(), proTxHash) != vPendingMasternodes.end()) {
        return false;
    }
    vPendingMasternodes.emplace_back(proTxHash);
    return true;
}

void TierTwoConnMan::addPendingProbeConnections(const std::set<uint256>& proTxHashes)
{
    LOCK(cs_vPendingMasternodes);
    masternodePendingProbes.insert(proTxHashes.begin(), proTxHashes.end());
}

void TierTwoConnMan::clear()
{
    LOCK(cs_vPendingMasternodes);
    masternodeQuorumNodes.clear();
    masternodeQuorumRelayMembers.clear();
    vPendingMasternodes.clear();
    masternodePendingProbes.clear();
}

void TierTwoConnMan::start(CScheduler& scheduler, const TierTwoConnMan::Options& options)
{
    // Must be started after connman
    assert(connman);
    interruptNet.reset();

    // Connecting to specific addresses, no masternode connections available
    if (options.m_has_specified_outgoing) return;
    // Initiate masternode connections
    threadOpenMasternodeConnections = std::thread(&TraceThread<std::function<void()> >, "mncon", std::function<void()>(std::bind(&TierTwoConnMan::ThreadOpenMasternodeConnections, this)));
    // Cleanup process every 60 seconds
    scheduler.scheduleEvery(std::bind(&TierTwoConnMan::doMaintenance, this), 60 * 1000);
}

void TierTwoConnMan::stop() {
    if (threadOpenMasternodeConnections.joinable()) {
        threadOpenMasternodeConnections.join();
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
    PeerData(const CService& s, bool disconnect, bool is_mn_conn) : service(s), f_disconnect(disconnect), f_is_mn_conn(is_mn_conn) {}
    const CService service;
    bool f_disconnect{false};
    bool f_is_mn_conn{false};
    bool operator==(const CService& s) const { return service == s; }
};

struct MnService {
public:
    uint256 verif_proreg_tx_hash{UINT256_ZERO};
    bool is_inbound{false};
    bool operator==(const uint256& hash) const { return verif_proreg_tx_hash == hash; }
};

void TierTwoConnMan::ThreadOpenMasternodeConnections()
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
        std::vector<MnService> connectedMnServices;
        connman->ForEachNode([&](const CNode* pnode) {
            connectedNodes.emplace_back(PeerData{pnode->addr, pnode->fDisconnect, pnode->m_masternode_connection});
            if (!pnode->verifiedProRegTxHash.IsNull()) {
                connectedMnServices.emplace_back(MnService{pnode->verifiedProRegTxHash, pnode->fInbound});
            }
        });

        // Try to connect to a single MN per cycle
        CDeterministicMNCPtr dmnToConnect{nullptr};
        // Current list
        auto mnList = deterministicMNManager->GetListAtChainTip();
        int64_t currentTime = GetAdjustedTime();
        bool isProbe = false;
        {
            LOCK(cs_vPendingMasternodes);

            // First try to connect to pending MNs
            if (!vPendingMasternodes.empty()) {
                auto dmn = mnList.GetValidMN(vPendingMasternodes.front());
                vPendingMasternodes.erase(vPendingMasternodes.begin());
                if (dmn) {
                    auto peerData = std::find(connectedNodes.begin(), connectedNodes.end(), dmn->pdmnState->addr);
                    if (peerData == std::end(connectedNodes)) {
                        dmnToConnect = dmn;
                        LogPrint(BCLog::NET_MN, "%s -- opening pending masternode connection to %s, service=%s\n",
                                 __func__, dmn->proTxHash.ToString(), dmn->pdmnState->addr.ToString());
                    }
                }
            }

            // Secondly, try to connect quorum members
            if (!dmnToConnect) {
                std::vector<CDeterministicMNCPtr> pending;
                for (const auto& group: masternodeQuorumNodes) {
                    for (const auto& proRegTxHash: group.second) {
                        // Skip if already have this member connected
                        if (std::count(connectedMnServices.begin(), connectedMnServices.end(), proRegTxHash) > 0) {
                            continue;
                        }

                        // Don't try to connect to ourselves
                        if (WITH_LOCK(cs_vPendingMasternodes, return local_dmn_pro_tx_hash && *local_dmn_pro_tx_hash == proRegTxHash)) {
                            continue;
                        }

                        // Check if DMN exists in tip list
                        const auto& dmn = mnList.GetValidMN(proRegTxHash);
                        if (!dmn) continue;
                        auto peerData = std::find(connectedNodes.begin(), connectedNodes.end(), dmn->pdmnState->addr);

                        // Skip already connected nodes.
                        if (peerData != std::end(connectedNodes) &&
                            (peerData->f_disconnect || peerData->f_is_mn_conn)) {
                            continue;
                        }

                        // Check if we already tried this connection recently to not retry too often
                        int64_t lastAttempt = g_mmetaman.GetMetaInfo(dmn->proTxHash)->GetLastOutboundAttempt();
                        // back off trying connecting to an address if we already tried recently
                        if (currentTime - lastAttempt < chainParams.LLMQConnectionRetryTimeout()) {
                            continue;
                        }
                        pending.emplace_back(dmn);
                    }
                }
                // Select a random node to connect
                if (!pending.empty()) {
                    dmnToConnect = pending[GetRandInt((int) pending.size())];
                    LogPrint(BCLog::NET_MN, "TierTwoConnMan::%s -- opening quorum connection to %s, service=%s\n",
                             __func__, dmnToConnect->proTxHash.ToString(), dmnToConnect->pdmnState->addr.ToString());
                }
            }

            // If no node was selected, let's try to probe nodes connection
            if (!dmnToConnect) {
                std::vector<CDeterministicMNCPtr> pending;
                for (auto it = masternodePendingProbes.begin(); it != masternodePendingProbes.end(); ) {
                    auto dmn = mnList.GetMN(*it);
                    if (!dmn) {
                        it = masternodePendingProbes.erase(it);
                        continue;
                    }

                    // Discard already connected outbound MNs
                    auto mnService = std::find(connectedMnServices.begin(), connectedMnServices.end(), dmn->proTxHash);
                    bool connectedAndOutbound = mnService != std::end(connectedMnServices) && !mnService->is_inbound;
                    if (connectedAndOutbound) {
                        // we already have an outbound connection to this MN so there is no eed to probe it again
                        g_mmetaman.GetMetaInfo(dmn->proTxHash)->SetLastOutboundSuccess(currentTime);
                        it = masternodePendingProbes.erase(it);
                        continue;
                    }

                    ++it;

                    int64_t lastAttempt = g_mmetaman.GetMetaInfo(dmn->proTxHash)->GetLastOutboundAttempt();
                    // back off trying connecting to an address if we already tried recently
                    if (currentTime - lastAttempt < chainParams.LLMQConnectionRetryTimeout()) {
                        continue;
                    }
                    pending.emplace_back(dmn);
                }

                // Select a random node to connect
                if (!pending.empty()) {
                    dmnToConnect = pending[GetRandInt((int)pending.size())];
                    masternodePendingProbes.erase(dmnToConnect->proTxHash);
                    isProbe = true;

                    LogPrint(BCLog::NET_MN, "CConnman::%s -- probing masternode %s, service=%s\n",
                             __func__, dmnToConnect->proTxHash.ToString(), dmnToConnect->pdmnState->addr.ToString());
                }
            }
        }

        // No DMN to connect
        if (!dmnToConnect || interruptNet) {
            continue;
        }

        // Update last attempt and try connection
        g_mmetaman.GetMetaInfo(dmnToConnect->proTxHash)->SetLastOutboundAttempt(currentTime);
        triedConnect = true;

        // Now connect
        openConnection(CAddress(dmnToConnect->pdmnState->addr, NODE_NETWORK), isProbe);
        // should be in the list now if connection was opened
        bool connected = connman->ForNode(dmnToConnect->pdmnState->addr, CConnman::AllNodes, [&](CNode* pnode) {
            if (pnode->fDisconnect) { LogPrintf("about to be disconnected\n");
                return false;
            }
            return true;
        });
        if (!connected) {
            LogPrint(BCLog::NET_MN, "TierTwoConnMan::%s -- connection failed for masternode  %s, service=%s\n",
                     __func__, dmnToConnect->proTxHash.ToString(), dmnToConnect->pdmnState->addr.ToString());
            // reset last outbound success
            g_mmetaman.GetMetaInfo(dmnToConnect->proTxHash)->SetLastOutboundSuccess(0);
        }
    }
}

static void ProcessMasternodeConnections(CConnman& connman, TierTwoConnMan& tierTwoConnMan)
{
    // Don't disconnect masternode connections when we have less than the desired amount of outbound nodes
    int nonMasternodeCount = 0;
    connman.ForEachNode([&](CNode* pnode) {
        if (!pnode->fInbound && !pnode->fFeeler && !pnode->fAddnode && !pnode->m_masternode_connection && !pnode->m_masternode_probe_connection) {
            nonMasternodeCount++;
        }
    });
    if (nonMasternodeCount < (int) connman.GetMaxOutboundNodeCount()) {
        return;
    }

    connman.ForEachNode([&](CNode* pnode) {
        // we're only disconnecting m_masternode_connection connections
        if (!pnode->m_masternode_connection) return;
        // we're only disconnecting outbound connections (inbound connections are disconnected in AcceptConnection())
        if (pnode->fInbound) return;
        // we're not disconnecting LLMQ connections
        if (tierTwoConnMan.isMasternodeQuorumNode(pnode)) return;
        // we're not disconnecting masternode probes for at least a few seconds
        if (pnode->m_masternode_probe_connection && GetSystemTimeInSeconds() - pnode->nTimeConnected < 5) return;

        if (fLogIPs) {
            LogPrintf("Closing Masternode connection: peer=%d, addr=%s\n", pnode->GetId(), pnode->addr.ToString());
        } else {
            LogPrintf("Closing Masternode connection: peer=%d\n", pnode->GetId());
        }
        pnode->fDisconnect = true;
    });
}

void TierTwoConnMan::doMaintenance()
{
    if(!g_tiertwo_sync_state.IsBlockchainSynced() || interruptNet) {
        return;
    }
    ProcessMasternodeConnections(*connman, *this);
}

