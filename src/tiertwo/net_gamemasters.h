// Copyright (c) 2020 The Dash developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_NET_GAMEMASTERS_H
#define hemis_NET_GAMEMASTERS_H

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

    // Remove the registered quorum from the pending/protected GM connections
    void removeQuorumNodes(Consensus::LLMQType llmqType, const uint256& quorumHash);

    // Add GMs to the active quorum relay members map and push QSENDRECSIGS to the verified connected peers that are part of this new quorum.
    void setGamemasterQuorumRelayMembers(Consensus::LLMQType llmqType, const uint256& quorumHash, const std::set<uint256>& proTxHashes);

    // Returns true if the node has the same address as a GM.
    bool isGamemasterQuorumNode(const CNode* pnode);

    // Whether protxHash an active quorum relay member
    bool isGamemasterQuorumRelayMember(const uint256& protxHash);

    // Add DGM to the pending connection list
    bool addPendingGamemaster(const uint256& proTxHash);

    // Adds the DGMs to the pending to probe list
    void addPendingProbeConnections(const std::set<uint256>& proTxHashes);

    // Set the local DGM so the node does not try to connect to himself
    void setLocalDGM(const uint256& pro_tx_hash) { WITH_LOCK(cs_vPendingGamemasters, local_dgm_pro_tx_hash = pro_tx_hash;); }

    // Clear connections cache
    void clear();

    // Manages the GM connections
    void ThreadOpenGamemasterConnections();
    void start(CScheduler& scheduler, const TierTwoConnMan::Options& options);
    void stop();
    void interrupt();

private:
    CThreadInterrupt interruptNet;
    std::thread threadOpenGamemasterConnections;

    mutable RecursiveMutex cs_vPendingGamemasters;
    std::vector<uint256> vPendingGamemasters GUARDED_BY(cs_vPendingGamemasters);
    typedef std::pair<Consensus::LLMQType, uint256> QuorumTypeAndHash;
    std::map<QuorumTypeAndHash, std::set<uint256>> gamemasterQuorumNodes GUARDED_BY(cs_vPendingGamemasters);
    std::map<QuorumTypeAndHash, std::set<uint256>> gamemasterQuorumRelayMembers GUARDED_BY(cs_vPendingGamemasters);
    std::set<uint256> gamemasterPendingProbes GUARDED_BY(cs_vPendingGamemasters);

    // The local DGM
    Optional<uint256> local_dgm_pro_tx_hash GUARDED_BY(cs_vPendingGamemasters){nullopt};

    // parent connections manager
    CConnman* connman;

    void openConnection(const CAddress& addrConnect, bool isProbe);
    void doMaintenance();
};

#endif //hemis_NET_GAMEMASTERS_H
