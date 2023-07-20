// Copyright (c) 2020 The Dash developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_NET_MASTERNODES_H
#define hemis_NET_MASTERNODES_H

#include "consensus/params.h"
#include "net.h"
#include "sync.h"
#include "threadinterrupt.h"
#include "uint256.h"

#include <thread>

class CAddress;
class CConnman;
class CChainParams;
class CNode;
class CScheduler;

class TierTwoConnMan
{
public:
    struct Options {
        bool m_has_specified_outgoing;
    };

    explicit TierTwoConnMan(CConnman* _connman);
    ~TierTwoConnMan();

    // Add or update quorum nodes
    void setQuorumNodes(Consensus::LLMQType llmqType,
                        const uint256& quorumHash,
                        const std::set<uint256>& proTxHashes);

    // Return quorum nodes for a given llmqType
    std::set<uint256> getQuorumNodes(Consensus::LLMQType llmqType);

    // Return quorum nodes for a given llmqType and hash
    std::set<NodeId> getQuorumNodes(Consensus::LLMQType llmqType, uint256 quorumHash);

    // Return true if the quorum was already registered
    bool hasQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash);

    // Remove the registered quorum from the pending/protected MN connections
    void removeQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash);

    // Add MNs to the active quorum relay members map and push QSENDRECSIGS to the verified connected peers that are part of this new quorum.
    void setMasternodeQuorumRelayMembers(Consensus::LLMQType llmqType, const uint256& quorumHash, const std::set<uint256>& proTxHashes);

    // Returns true if the node has the same address as a MN.
    bool isMasternodeQuorumNode(const CNode* pnode);

    // Whether protxHash an active quorum relay member
    bool isMasternodeQuorumRelayMember(const uint256& protxHash);

    // Add DMN to the pending connection list
    bool addPendingMasternode(const uint256& proTxHash);

    // Adds the DMNs to the pending to probe list
    void addPendingProbeConnections(const std::set<uint256>& proTxHashes);

    // Set the local DMN so the node does not try to connect to himself
    void setLocalDMN(const uint256& pro_tx_hash) { WITH_LOCK(cs_vPendingMasternodes, local_dmn_pro_tx_hash = pro_tx_hash;); }

    // Clear connections cache
    void clear();

    // Manages the MN connections
    void ThreadOpenMasternodeConnections();
    void start(CScheduler& scheduler, const TierTwoConnMan::Options& options);
    void stop();
    void interrupt();

private:
    CThreadInterrupt interruptNet;
    std::thread threadOpenMasternodeConnections;

    mutable RecursiveMutex cs_vPendingMasternodes;
    std::vector<uint256> vPendingMasternodes GUARDED_BY(cs_vPendingMasternodes);
    typedef std::pair<Consensus::LLMQType, uint256> QuorumTypeAndHash;
    std::map<QuorumTypeAndHash, std::set<uint256>> masternodeQuorumNodes GUARDED_BY(cs_vPendingMasternodes);
    std::map<QuorumTypeAndHash, std::set<uint256>> masternodeQuorumRelayMembers GUARDED_BY(cs_vPendingMasternodes);
    std::set<uint256> masternodePendingProbes GUARDED_BY(cs_vPendingMasternodes);

    // The local DMN
    Optional<uint256> local_dmn_pro_tx_hash GUARDED_BY(cs_vPendingMasternodes){nullopt};

    // parent connections manager
    CConnman* connman;

    void openConnection(const CAddress& addrConnect, bool isProbe);
    void doMaintenance();
};

#endif //hemis_NET_MASTERNODES_H
