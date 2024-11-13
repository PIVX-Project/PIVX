// Copyright (c) 2018 The Dash Core developers
// Copyright (c) 2023 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_LLMQ_QUORUMS_H
#define PIVX_LLMQ_QUORUMS_H

#include "llmq/quorums_commitment.h"
#include "bls/bls_worker.h"
#include "bls/bls_wrapper.h"
#include "consensus/params.h"
#include "evo/deterministicmns.h"
#include "evo/evodb.h"
#include "saltedhasher.h"
#include "unordered_lru_cache.h"
#include "validationinterface.h"

namespace llmq
{

class CDKGSessionManager;

/**
 * An object of this class represents a quorum which was mined on-chain (through a quorum commitment)
 * It at least contains information about the members and the quorum public key which is needed to verify recovered
 * signatures from this quorum.
 *
 * In case the local node is a member of the same quorum and successfully participated in the DKG, the quorum object
 * will also contain the secret key share and the quorum verification vector. The quorum vvec is then used to recover
 * the public key shares of individual members, which are needed to verify signature shares of these members.
 */
class CQuorum
{
    friend class CQuorumManager;

public:
    const Consensus::LLMQParams& params;
    const CBlockIndex* pindexQuorum;
    CFinalCommitment qc;
    int height;
    uint256 minedBlockHash;
    std::vector<CDeterministicMNCPtr> members;

    // These are only valid when we either participated in the DKG or fully watched it
    BLSVerificationVectorPtr quorumVvec;
    CBLSSecretKey skShare;

private:
    // Recovery of public key shares is very slow, so we start a background thread that pre-populates a cache so that
    // the public key shares are ready when needed later
    mutable CBLSWorkerCache blsCache;
    std::atomic<bool> stopCachePopulatorThread;
    std::thread cachePopulatorThread;

public:
    CQuorum(const Consensus::LLMQParams& _params, CBLSWorker& _blsWorker) : params(_params), blsCache(_blsWorker), stopCachePopulatorThread(false) {}
    ~CQuorum();
    void Init(const CFinalCommitment& _qc, const CBlockIndex* pindexQuorum, int _height, const uint256& _minedBlockHash, const std::vector<CDeterministicMNCPtr>& _members);

    bool IsMember(const uint256& proTxHash) const;
    bool IsValidMember(const uint256& proTxHash) const;
    int GetMemberIndex(const uint256& proTxHash) const;

    CBLSPublicKey GetPubKeyShare(size_t memberIdx) const;
    CBLSSecretKey GetSkShare() const;

private:
    void WriteContributions(CEvoDB& evoDb);
    bool ReadContributions(CEvoDB& evoDb);
    static void StartCachePopulatorThread(std::shared_ptr<CQuorum> _this);
};
typedef std::shared_ptr<CQuorum> CQuorumPtr;
typedef std::shared_ptr<const CQuorum> CQuorumCPtr;

/**
 * The quorum manager maintains quorums which were mined on chain. When a quorum is requested from the manager,
 * it will lookup the commitment (through CQuorumBlockProcessor) and build a CQuorum object from it.
 *
 * It is also responsible for initialization of the inter-quorum connections for new quorums.
 */
class CQuorumManager
{
private:
    CEvoDB& evoDb;
    CBLSWorker& blsWorker;
    CDKGSessionManager& dkgManager;

    RecursiveMutex quorumsCacheCs;
    mutable std::map<Consensus::LLMQType, unordered_lru_cache<uint256, CQuorumCPtr, StaticSaltedHasher>> mapQuorumsCache;
    mutable std::map<Consensus::LLMQType, unordered_lru_cache<uint256, std::vector<CQuorumCPtr>, StaticSaltedHasher>> scanQuorumsCache;

public:
    CQuorumManager(CEvoDB& _evoDb, CBLSWorker& _blsWorker, CDKGSessionManager& _dkgManager);

    void UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload);

public:
    bool HasQuorum(Consensus::LLMQType llmqType, const uint256& quorumHash);

    // all these methods will lock cs_main
    CQuorumCPtr GetQuorum(Consensus::LLMQType llmqType, const uint256& quorumHash);

    std::vector<CQuorumCPtr> ScanQuorums(Consensus::LLMQType llmqType, size_t maxCount);

    // this method will not lock cs_main
    std::vector<CQuorumCPtr> ScanQuorums(Consensus::LLMQType llmqType, const CBlockIndex* pindexStart, size_t maxCount);

private:
    // all these methods will not lock cs_main
    void EnsureQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexNew);

    bool BuildQuorumFromCommitment(const CFinalCommitment& qc, const CBlockIndex* pindexQuorum, const uint256& minedBlockHash, std::shared_ptr<CQuorum>& quorum) const;
    bool BuildQuorumContributions(const CFinalCommitment& fqc, std::shared_ptr<CQuorum>& quorum) const;

    CQuorumCPtr GetQuorum(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum);
};

extern std::unique_ptr<CQuorumManager> quorumManager;
} // namespace llmq

#endif // PIVX_LLMQ_QUORUMS_H
