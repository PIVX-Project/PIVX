// Copyright (c) 2019 The Dash Core developers
// Copyright (c) 2023 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_QUORUMS_CHAINLOCKS_H
#define hemis_QUORUMS_CHAINLOCKS_H

#include "llmq/quorums.h"
#include "llmq/quorums_signing.h"

#include "net.h"
#include "chainparams.h"

#include <atomic>

class CBlockIndex;
class CScheduler;

namespace llmq
{

class CChainLockSig
{
public:
    int32_t nHeight{-1};
    uint256 blockHash;
    CBLSSignature sig;

public:
    SERIALIZE_METHODS(CChainLockSig, obj)
    {
        READWRITE(obj.nHeight);
        READWRITE(obj.blockHash);
        READWRITE(obj.sig);
    }
    std::string ToString() const;
};

class CChainLocksHandler : public CRecoveredSigsListener
{
    static const int64_t CLEANUP_INTERVAL = 1000 * 30;
    static const int64_t CLEANUP_SEEN_TIMEOUT = 24 * 60 * 60 * 1000;

private:
    CScheduler* scheduler;
    RecursiveMutex cs;
    std::atomic<bool> inEnforceBestChainLock{false};

    uint256 bestChainLockHash;
    CChainLockSig bestChainLock;

    CChainLockSig bestChainLockWithKnownBlock;
    const CBlockIndex* bestChainLockBlockIndex{nullptr};

    int32_t lastSignedHeight{-1};
    uint256 lastSignedRequestId;
    uint256 lastSignedMsgHash;

    std::map<uint256, int64_t> seenChainLocks;

    int64_t lastCleanupTime{0};

public:
    CChainLocksHandler(CScheduler* _scheduler);
    ~CChainLocksHandler();

public:
    bool AlreadyHave(const CInv& inv);
    bool GetChainLockByHash(const uint256& hash, CChainLockSig& ret);

    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    void ProcessNewChainLock(NodeId from, const CChainLockSig& clsig, const uint256& hash);
    void AcceptedBlockHeader(const CBlockIndex* pindexNew);
    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork);
    void EnforceBestChainLock();
    virtual void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig);

    bool HasChainLock(int nHeight, const uint256& blockHash);
    bool HasConflictingChainLock(int nHeight, const uint256& blockHash);

private:
    // these require locks to be held already
    bool InternalHasChainLock(int nHeight, const uint256& blockHash);
    bool InternalHasConflictingChainLock(int nHeight, const uint256& blockHash);

    void ScheduleInvalidateBlock(const CBlockIndex* pindex);
    void DoInvalidateBlock(const CBlockIndex* pindex, bool activateBestChain);

    void Cleanup();
};

extern CChainLocksHandler* chainLocksHandler;


}

#endif //hemis_QUORUMS_CHAINLOCKS_H
