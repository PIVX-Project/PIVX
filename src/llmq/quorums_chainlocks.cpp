// Copyright (c) 2019 The Dash Core developers
// Copyright (c) 2023 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "quorums.h"
#include "quorums_chainlocks.h"
#include "quorums_signing.h"
#include "quorums_utils.h"

#include "chain.h"
#include "net_processing.h"
#include "scheduler.h"
#include "spork.h"
#include "sporkid.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "validation.h"

namespace llmq
{

static const std::string CLSIG_REQUESTID_PREFIX = "clsig";

std::unique_ptr<CChainLocksHandler> chainLocksHandler{nullptr};

bool CChainLockSig::IsNull() const
{
    return nHeight == -1 && blockHash == uint256();
}

std::string CChainLockSig::ToString() const
{
    return strprintf("CChainLockSig(nHeight=%d, blockHash=%s)", nHeight, blockHash.ToString());
}

CChainLocksHandler::CChainLocksHandler(CScheduler* _scheduler) :
    scheduler(_scheduler)
{
}

CChainLocksHandler::~CChainLocksHandler()
{
}

void CChainLocksHandler::Start()
{
    quorumSigningManager->RegisterRecoveredSigsListener(this);
    scheduler->scheduleEvery([&]() {
        EnforceBestChainLock();
        // regularely retry signing the current chaintip
        TrySignChainTip();
    },
        5000);
}

void CChainLocksHandler::Stop()
{
    quorumSigningManager->UnregisterRecoveredSigsListener(this);
}

bool CChainLocksHandler::AlreadyHave(const CInv& inv)
{
    LOCK(cs);
    return seenChainLocks.count(inv.hash) != 0;
}

bool CChainLocksHandler::GetChainLockByHash(const uint256& hash, llmq::CChainLockSig& ret)
{
    LOCK(cs);

    if (hash != bestChainLockHash) {
        // we only propagate the best one and ditch all the old ones
        return false;
    }

    ret = bestChainLock;
    return true;
}

CChainLockSig CChainLocksHandler::GetBestChainLock()
{
    LOCK(cs);
    return bestChainLock;
}

void CChainLocksHandler::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    if (strCommand == NetMsgType::CLSIG) {
        CChainLockSig clsig;
        vRecv >> clsig;

        auto hash = ::SerializeHash(clsig);

        ProcessNewChainLock(pfrom->GetId(), clsig, hash);
    }
}

void CChainLocksHandler::ProcessNewChainLock(NodeId from, const llmq::CChainLockSig& clsig, const uint256& hash)
{
    {
        LOCK(cs_main);
        g_connman->RemoveAskFor(hash, MSG_CLSIG);
    }

    {
        LOCK(cs);
        if (!seenChainLocks.emplace(hash, GetTimeMillis()).second) {
            return;
        }

        if (!bestChainLock.IsNull() && clsig.nHeight <= bestChainLock.nHeight) {
            // no need to process/relay older CLSIGs
            return;
        }
    }

    uint256 requestId = ::SerializeHash(std::make_pair(CLSIG_REQUESTID_PREFIX, clsig.nHeight));
    uint256 msgHash = clsig.blockHash;
    if (!quorumSigningManager->VerifyRecoveredSig(Params().GetConsensus().llmqTypeChainLocks, clsig.nHeight, requestId, msgHash, clsig.sig)) {
        LogPrintf("CChainLocksHandler::%s -- invalid CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
        if (from != -1) {
            LOCK(cs_main);
            Misbehaving(from, 10);
        }
        return;
    }

    {
        LOCK2(cs_main, cs);

        if (InternalHasConflictingChainLock(clsig.nHeight, clsig.blockHash)) {
            // This should not happen. If it happens, it means that a malicious entity controls a large part of the MN
            // network. In this case, we don't allow him to reorg older chainlocks.
            LogPrintf("CChainLocksHandler::%s -- new CLSIG (%s) tries to reorg previous CLSIG (%s), peer=%d\n",
                      __func__, clsig.ToString(), bestChainLock.ToString(), from);
            return;
        }

        bestChainLockHash = hash;
        bestChainLock = clsig;

        CInv inv(MSG_CLSIG, hash);
        g_connman->RelayInv(inv, LLMQS_PROTO_VERSION);

        auto blockIt = mapBlockIndex.find(clsig.blockHash);
        if (blockIt == mapBlockIndex.end()) {
            // we don't know the block/header for this CLSIG yet, so bail out for now
            // when the block or the header later comes in, we will enforce the correct chain
            return;
        }

        if (blockIt->second->nHeight != clsig.nHeight) {
            // Should not happen, same as the conflict check from above.
            LogPrintf("CChainLocksHandler::%s -- height of CLSIG (%s) does not match the specified block's height (%d)\n",
                    __func__, clsig.ToString(), blockIt->second->nHeight);
            return;
        }

        const CBlockIndex* pindex = blockIt->second;
        bestChainLockWithKnownBlock = bestChainLock;
        bestChainLockBlockIndex = pindex;
    }

    scheduler->scheduleFromNow([&]() {
        EnforceBestChainLock();
    },
        0);

    LogPrint(BCLog::LLMQ, "CChainLocksHandler::%s -- processed new CLSIG (%s), peer=%d\n",
        __func__, clsig.ToString(), from);
}

void CChainLocksHandler::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    LOCK2(cs_main, cs);

    if (pindexNew->GetBlockHash() == bestChainLock.blockHash) {
        LogPrintf("CChainLocksHandler::%s -- block header %s came in late, updating and enforcing\n", __func__, pindexNew->GetBlockHash().ToString());

        if (bestChainLock.nHeight != pindexNew->nHeight) {
            // Should not happen, same as the conflict check from ProcessNewChainLock.
            LogPrintf("CChainLocksHandler::%s -- height of CLSIG (%s) does not match the specified block's height (%d)\n",
                __func__, bestChainLock.ToString(), pindexNew->nHeight);
            return;
        }

        // when EnforceBestChainLock is called later, it might end up invalidating other chains but not activating the
        // CLSIG locked chain. This happens when only the header is known but the block is still missing yet. The usual
        // block processing logic will handle this when the block arrives
        bestChainLockWithKnownBlock = bestChainLock;
        bestChainLockBlockIndex = pindexNew;
    }
}

void CChainLocksHandler::UpdatedBlockTip(const CBlockIndex* pindexNew)
{
    // don't call TrySignChainTip directly but instead let the scheduler call it. This way we ensure that cs_main is
    // never locked and TrySignChainTip is not called twice in parallel. Also avoids recursive calls due to
    // EnforceBestChainLock switching chains.
    LOCK(cs);
    if (tryLockChainTipScheduled) {
        return;
    }
    tryLockChainTipScheduled = true;
    scheduler->scheduleFromNow([&]() {
        EnforceBestChainLock();
        TrySignChainTip();
        LOCK(cs);
        tryLockChainTipScheduled = false;
    },
        0);
}

void CChainLocksHandler::TrySignChainTip()
{
    Cleanup();

    if (!fMasterNode) {
        return;
    }

    if (!g_tiertwo_sync_state.IsBlockchainSynced()) {
        return;
    }

    const CBlockIndex* pindex;
    {
        LOCK(cs_main);
        pindex = chainActive.Tip();
    }

    if (!pindex->pprev) {
        return;
    }
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    // DIP8 defines a process called "Signing attempts" which should run before the CLSIG is finalized
    // To simplify the initial implementation, we skip this process and directly try to create a CLSIG
    // This will fail when multiple blocks compete, but we accept this for the initial implementation.
    // Later, we'll add the multiple attempts process.

    {
        LOCK(cs);

        if (pindex->nHeight == lastSignedHeight) {
            // already signed this one
            return;
        }

        if (bestChainLock.nHeight >= pindex->nHeight) {
            // already got the same CLSIG or a better one
            return;
        }

        if (InternalHasConflictingChainLock(pindex->nHeight, pindex->GetBlockHash())) {
            // don't sign if another conflicting CLSIG is already present. EnforceBestChainLock will later enforce
            // the correct chain.
            return;
        }
    }

    LogPrint(BCLog::LLMQ, "CChainLocksHandler::%s -- trying to sign %s, height=%d\n", __func__, pindex->GetBlockHash().ToString(), pindex->nHeight);

    uint256 requestId = ::SerializeHash(std::make_pair(CLSIG_REQUESTID_PREFIX, pindex->nHeight));
    uint256 msgHash = pindex->GetBlockHash();

    {
        LOCK(cs);
        if (bestChainLock.nHeight >= pindex->nHeight) {
            // might have happened while we didn't hold cs
            return;
        }
        lastSignedHeight = pindex->nHeight;
        lastSignedRequestId = requestId;
        lastSignedMsgHash = msgHash;
    }

    quorumSigningManager->AsyncSignIfMember(Params().GetConsensus().llmqTypeChainLocks, requestId, msgHash);
}

// WARNING: cs_main and cs should not be held!
// This should also not be called from validation signals, as this might result in recursive calls
void CChainLocksHandler::EnforceBestChainLock()
{
    AssertLockNotHeld(cs);
    AssertLockNotHeld(cs_main);

    CChainLockSig clsig;
    const CBlockIndex* pindex;
    const CBlockIndex* currentBestChainLockBlockIndex;
    {
        LOCK(cs);
        clsig = bestChainLockWithKnownBlock;
        pindex = currentBestChainLockBlockIndex = this->bestChainLockBlockIndex;

        if (!currentBestChainLockBlockIndex) {
            // we don't have the header/block, so we can't do anything right now
            return;
        }
    }

    bool activateNeeded;
    {
        LOCK(cs_main);

        // Go backwards through the chain referenced by clsig until we find a block that is part of the main chain.
        // For each of these blocks, check if there are children that are NOT part of the chain referenced by clsig
        // and invalidate each of them.
        while (pindex && !chainActive.Contains(pindex)) {
            // Invalidate all blocks that have the same prevBlockHash but are not equal to blockHash
            auto itp = mapPrevBlockIndex.equal_range(pindex->pprev->GetBlockHash());
            for (auto jt = itp.first; jt != itp.second; ++jt) {
                if (jt->second == pindex) {
                    continue;
                }
                LogPrintf("CChainLocksHandler::%s -- CLSIG (%s) invalidates block %s\n",
                    __func__, clsig.ToString(), jt->second->GetBlockHash().ToString());
                DoInvalidateBlock(jt->second, false);
            }

            pindex = pindex->pprev;
        }
        // In case blocks from the correct chain are invalid at the moment, reconsider them. The only case where this
        // can happen right now is when missing superblock triggers caused the main chain to be dismissed first. When
        // the trigger later appears, this should bring us to the correct chain eventually. Please note that this does
        // NOT enforce invalid blocks in any way, it just causes re-validation.
        if (!currentBestChainLockBlockIndex->IsValid()) {
            CValidationState state;
            ReconsiderBlock(state, mapBlockIndex.at(currentBestChainLockBlockIndex->GetBlockHash()));
        }

        activateNeeded = chainActive.Tip()->GetAncestor(currentBestChainLockBlockIndex->nHeight) != currentBestChainLockBlockIndex;
    }

    CValidationState state;
    if (activateNeeded && !ActivateBestChain(state)) {
        LogPrintf("CChainLocksHandler::%s -- ActivateBestChain failed: %s\n", __func__, state.GetRejectReason());
    }
}

void CChainLocksHandler::HandleNewRecoveredSig(const llmq::CRecoveredSig& recoveredSig)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return;
    }

    CChainLockSig clsig;
    {
        LOCK(cs);

        if (recoveredSig.id != lastSignedRequestId || recoveredSig.msgHash != lastSignedMsgHash) {
            // this is not what we signed, so lets not create a CLSIG for it
            return;
        }
        if (bestChainLock.nHeight >= lastSignedHeight) {
            // already got the same or a better CLSIG through the CLSIG message
            return;
        }

        clsig.nHeight = lastSignedHeight;
        clsig.blockHash = lastSignedMsgHash;
        clsig.sig = recoveredSig.sig.Get();
    }
    ProcessNewChainLock(-1, clsig, ::SerializeHash(clsig));
}

// WARNING, do not hold cs while calling this method as we'll otherwise run into a deadlock
void CChainLocksHandler::DoInvalidateBlock(const CBlockIndex* pindex, bool activateBestChain)
{
    auto& params = Params();

    {
        LOCK(cs_main);

        // get the non-const pointer
        CBlockIndex* pindex2 = mapBlockIndex[pindex->GetBlockHash()];

        CValidationState state;
        if (!InvalidateBlock(state, params, pindex2)) {
            LogPrintf("CChainLocksHandler::%s -- InvalidateBlock failed: %s\n", __func__, state.GetRejectReason());
            // This should not have happened and we are in a state were it's not safe to continue anymore
            assert(false);
        }
    }

    CValidationState state;
    if (activateBestChain && !ActivateBestChain(state)) {
        LogPrintf("CChainLocksHandler::%s -- ActivateBestChain failed: %s\n", __func__, state.GetRejectReason());
        // This should not have happened and we are in a state were it's not safe to continue anymore
        assert(false);
    }
}

bool CChainLocksHandler::HasChainLock(int nHeight, const uint256& blockHash)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return false;
    }

    LOCK(cs);
    return InternalHasChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash == bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    return pAncestor && pAncestor->GetBlockHash() == blockHash;
}

bool CChainLocksHandler::HasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    if (!sporkManager.IsSporkActive(SPORK_23_CHAINLOCKS_ENFORCEMENT)) {
        return false;
    }

    LOCK(cs);
    return InternalHasConflictingChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash != bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    assert(pAncestor);
    return pAncestor->GetBlockHash() != blockHash;
}

void CChainLocksHandler::Cleanup()
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) {
        return;
    }

    {
        LOCK(cs);
        if (GetTimeMillis() - lastCleanupTime < CLEANUP_INTERVAL) {
            return;
        }
    }

    LOCK2(cs_main, cs);

    for (auto it = seenChainLocks.begin(); it != seenChainLocks.end(); ) {
        if (GetTimeMillis() - it->second >= CLEANUP_SEEN_TIMEOUT) {
            it = seenChainLocks.erase(it);
        } else {
            ++it;
        }
    }

    lastCleanupTime = GetTimeMillis();
}

}
