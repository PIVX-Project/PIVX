// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// clang-format off
#include "activegamemaster.h"
#include "addrman.h"
#include "budget/budgetmanager.h"
#include "evo/deterministicgms.h"
#include "gamemaster-sync.h"
#include "gamemaster.h"
#include "gamemasterman.h"
#include "netmessagemaker.h"
#include "tiertwo/netfulfilledman.h"
#include "spork.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "util/system.h"
#include "validation.h"
// clang-format on

class CGamemasterSync;
CGamemasterSync gamemasterSync;

CGamemasterSync::CGamemasterSync()
{
    Reset();
}

bool CGamemasterSync::NotCompleted()
{
    return (!g_tiertwo_sync_state.IsSynced() && (
            !g_tiertwo_sync_state.IsSporkListSynced() ||
            sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT) ||
            sporkManager.IsSporkActive(SPORK_9_GAMEMASTER_BUDGET_ENFORCEMENT) ||
            sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)));
}

void CGamemasterSync::UpdateBlockchainSynced(bool isRegTestNet)
{
    if (!isRegTestNet && !g_tiertwo_sync_state.CanUpdateChainSync(lastProcess)) return;
    if (fImporting || fReindex) return;

    int64_t blockTime = 0;
    {
        TRY_LOCK(g_best_block_mutex, lock);
        if (!lock) return;
        blockTime = g_best_block_time;
    }

    // Synced only if the last block happened in the last 60 minutes
    bool is_chain_synced = blockTime + 60 * 60 > lastProcess;
    g_tiertwo_sync_state.SetBlockchainSync(is_chain_synced, lastProcess);
}

void CGamemasterSync::Reset()
{
    g_tiertwo_sync_state.SetBlockchainSync(false, 0);
    g_tiertwo_sync_state.ResetData();
    lastProcess = 0;
    lastFailure = 0;
    nCountFailures = 0;
    sumGamemasterList = 0;
    sumGamemasterWinner = 0;
    sumBudgetItemProp = 0;
    sumBudgetItemFin = 0;
    countGamemasterList = 0;
    countGamemasterWinner = 0;
    countBudgetItemProp = 0;
    countBudgetItemFin = 0;
    g_tiertwo_sync_state.SetCurrentSyncPhase(GAMEMASTER_SYNC_INITIAL);
    RequestedGamemasterAttempt = 0;
    nAssetSyncStarted = GetTime();
}

bool CGamemasterSync::IsBudgetPropEmpty()
{
    return sumBudgetItemProp == 0 && countBudgetItemProp > 0;
}

bool CGamemasterSync::IsBudgetFinEmpty()
{
    return sumBudgetItemFin == 0 && countBudgetItemFin > 0;
}

int CGamemasterSync::GetNextAsset(int currentAsset)
{
    if (currentAsset > GAMEMASTER_SYNC_FINISHED) {
        LogPrintf("%s - invalid asset %d\n", __func__, currentAsset);
        return GAMEMASTER_SYNC_FAILED;
    }
    switch (currentAsset) {
    case (GAMEMASTER_SYNC_INITIAL):
    case (GAMEMASTER_SYNC_FAILED):
        return GAMEMASTER_SYNC_SPORKS;
    case (GAMEMASTER_SYNC_SPORKS):
        return deterministicGMManager->LegacyGMObsolete() ? GAMEMASTER_SYNC_BUDGET : GAMEMASTER_SYNC_LIST;
    case (GAMEMASTER_SYNC_LIST):
        return deterministicGMManager->LegacyGMObsolete() ? GAMEMASTER_SYNC_BUDGET : GAMEMASTER_SYNC_GMW;
    case (GAMEMASTER_SYNC_GMW):
        return GAMEMASTER_SYNC_BUDGET;
    case (GAMEMASTER_SYNC_BUDGET):
    default:
        return GAMEMASTER_SYNC_FINISHED;
    }
}

void CGamemasterSync::SwitchToNextAsset()
{
    int RequestedGamemasterAssets = g_tiertwo_sync_state.GetSyncPhase();
    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_INITIAL ||
            RequestedGamemasterAssets == GAMEMASTER_SYNC_FAILED) {
        ClearFulfilledRequest();
    }
    const int nextAsset = GetNextAsset(RequestedGamemasterAssets);
    if (nextAsset == GAMEMASTER_SYNC_FINISHED) {
        LogPrintf("%s - Sync has finished\n", __func__);
    }
    g_tiertwo_sync_state.SetCurrentSyncPhase(nextAsset);
    RequestedGamemasterAttempt = 0;
    nAssetSyncStarted = GetTime();
}

std::string CGamemasterSync::GetSyncStatus()
{
    switch (g_tiertwo_sync_state.GetSyncPhase()) {
    case GAMEMASTER_SYNC_INITIAL:
        return _("GMs synchronization pending...");
    case GAMEMASTER_SYNC_SPORKS:
        return _("Synchronizing sporks...");
    case GAMEMASTER_SYNC_LIST:
        return _("Synchronizing gamemasters...");
    case GAMEMASTER_SYNC_GMW:
        return _("Synchronizing gamemaster winners...");
    case GAMEMASTER_SYNC_BUDGET:
        return _("Synchronizing budgets...");
    case GAMEMASTER_SYNC_FAILED:
        return _("Synchronization failed");
    case GAMEMASTER_SYNC_FINISHED:
        return _("Synchronization finished");
    }
    return "";
}

void CGamemasterSync::ProcessSyncStatusMsg(int nItemID, int nCount)
{
    int RequestedGamemasterAssets = g_tiertwo_sync_state.GetSyncPhase();
    if (RequestedGamemasterAssets >= GAMEMASTER_SYNC_FINISHED) return;

    //this means we will receive no further communication
    switch (nItemID) {
        case (GAMEMASTER_SYNC_LIST):
            if (nItemID != RequestedGamemasterAssets) return;
            sumGamemasterList += nCount;
            countGamemasterList++;
            break;
        case (GAMEMASTER_SYNC_GMW):
            if (nItemID != RequestedGamemasterAssets) return;
            sumGamemasterWinner += nCount;
            countGamemasterWinner++;
            break;
        case (GAMEMASTER_SYNC_BUDGET_PROP):
            if (RequestedGamemasterAssets != GAMEMASTER_SYNC_BUDGET) return;
            sumBudgetItemProp += nCount;
            countBudgetItemProp++;
            break;
        case (GAMEMASTER_SYNC_BUDGET_FIN):
            if (RequestedGamemasterAssets != GAMEMASTER_SYNC_BUDGET) return;
            sumBudgetItemFin += nCount;
            countBudgetItemFin++;
            break;
        default:
            break;
    }

    LogPrint(BCLog::GAMEMASTER, "CGamemasterSync:ProcessMessage - ssc - got inventory count %d %d\n", nItemID, nCount);
}

void CGamemasterSync::ClearFulfilledRequest()
{
    g_netfulfilledman.Clear();
}

void CGamemasterSync::Process()
{
    static int tick = 0;
    const bool isRegTestNet = Params().IsRegTestNet();

    if (tick++ % GAMEMASTER_SYNC_TIMEOUT != 0) return;

    // if the last call to this function was more than 60 minutes ago (client was in sleep mode)
    // reset the sync process
    int64_t now = GetTime();
    if (lastProcess != 0 && now > lastProcess + 60 * 60) {
        Reset();
    }
    lastProcess = now;

    // Update chain sync status using the 'lastProcess' time
    UpdateBlockchainSynced(isRegTestNet);

    if (g_tiertwo_sync_state.IsSynced()) {
        if (isRegTestNet) {
            return;
        }
        bool legacy_obsolete = deterministicGMManager->LegacyGMObsolete();
        // Check if we lost all gamemasters (except the local one in case the node is a GM)
        // from sleep/wake or failure to sync originally (after spork 21, check if we lost
        // all proposals instead). If we did, resync from scratch.
        if ((!legacy_obsolete && gamemasterman.CountEnabled(true /* only_legacy */) <= 1) ||
            (legacy_obsolete && g_budgetman.CountProposals() == 0)) {
            Reset();
        } else {
            return;
        }
    }

    // Try syncing again
    int RequestedGamemasterAssets = g_tiertwo_sync_state.GetSyncPhase();
    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_FAILED && lastFailure + (1 * 60) < GetTime()) {
        Reset();
    } else if (RequestedGamemasterAssets == GAMEMASTER_SYNC_FAILED) {
        return;
    }

    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_INITIAL) SwitchToNextAsset();

    // sporks synced but blockchain is not, wait until we're almost at a recent block to continue
    if (!g_tiertwo_sync_state.IsBlockchainSynced() &&
        RequestedGamemasterAssets > GAMEMASTER_SYNC_SPORKS) return;

    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    bool fLegacyGmObsolete = deterministicGMManager->LegacyGMObsolete();

    CGamemasterSync* sync = this;


    g_connman->ForEachNode([sync](CNode* pnode){
        return sync->SyncRegtest(pnode);
    });

    // Mainnet sync
    g_connman->ForEachNodeInRandomOrderContinueIf([sync, fLegacyGmObsolete](CNode* pnode){
        return sync->SyncWithNode(pnode, fLegacyGmObsolete);
    });
}

void CGamemasterSync::syncTimeout(const std::string& reason)
{
    LogPrintf("%s - ERROR - Sync has failed on %s, will retry later\n", __func__, reason);
    g_tiertwo_sync_state.SetCurrentSyncPhase(GAMEMASTER_SYNC_FAILED);
    RequestedGamemasterAttempt = 0;
    lastFailure = GetTime();
    nCountFailures++;
}

bool CGamemasterSync::SyncWithNode(CNode* pnode, bool fLegacyGmObsolete)
{
    int RequestedGamemasterAssets = g_tiertwo_sync_state.GetSyncPhase();
    CNetMsgMaker msgMaker(pnode->GetSendVersion());

    //set to synced
    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_SPORKS) {

        // Sync sporks from at least 2 peers
        if (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD) {
            SwitchToNextAsset();
            return false;
        }

        // Request sporks sync if we haven't requested it yet.
        if (g_netfulfilledman.HasFulfilledRequest(pnode->addr, "getspork")) return true;
        g_netfulfilledman.AddFulfilledRequest(pnode->addr, "getspork");

        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETSPORKS));
        RequestedGamemasterAttempt++;
        return false;
    }

    if (pnode->nVersion < ActiveProtocol() || !pnode->CanRelay()) {
        return true; // move to next peer
    }

    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_LIST) {
        if (fLegacyGmObsolete) {
            SwitchToNextAsset();
            return false;
        }

        int lastGamemasterList = g_tiertwo_sync_state.GetlastGamemasterList();
        LogPrint(BCLog::GAMEMASTER, "CGamemasterSync::Process() - lastGamemasterList %lld (GetTime() - GAMEMASTER_SYNC_TIMEOUT) %lld\n", lastGamemasterList, GetTime() - GAMEMASTER_SYNC_TIMEOUT);
        if (lastGamemasterList > 0 && lastGamemasterList < GetTime() - GAMEMASTER_SYNC_TIMEOUT * 8 && RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD) {
            // hasn't received a new item in the last 40 seconds AND has sent at least a minimum of GAMEMASTER_SYNC_THRESHOLD GETGMLIST requests,
            // so we'll move to the next asset.
            SwitchToNextAsset();
            return false;
        }

        // timeout
        if (lastGamemasterList == 0 &&
            (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 3 || GetTime() - nAssetSyncStarted > GAMEMASTER_SYNC_TIMEOUT * 5)) {
            if (sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT)) {
                syncTimeout("GAMEMASTER_SYNC_LIST");
            } else {
                SwitchToNextAsset();
            }
            return false;
        }

        // Don't request gmlist initial sync to more than 8 randomly ordered peers in this round
        if (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 4) return false;

        // Request gmb sync if we haven't requested it yet.
        if (g_netfulfilledman.HasFulfilledRequest(pnode->addr, "gmsync")) return true;

        // Try to request GM list sync.
        if (!gamemasterman.RequestGmList(pnode)) {
            return true; // Failed, try next peer.
        }

        // Mark sync requested.
        g_netfulfilledman.AddFulfilledRequest(pnode->addr, "gmsync");
        // Increase the sync attempt count
        RequestedGamemasterAttempt++;

        return false; // sleep 1 second before do another request round.
    }

    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_GMW) {
        if (fLegacyGmObsolete) {
            SwitchToNextAsset();
            return false;
        }

        int lastGamemasterWinner = g_tiertwo_sync_state.GetlastGamemasterWinner();
        if (lastGamemasterWinner > 0 && lastGamemasterWinner < GetTime() - GAMEMASTER_SYNC_TIMEOUT * 2 && RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD) { //hasn't received a new item in the last five seconds, so we'll move to the
            SwitchToNextAsset();
            // in case we received a budget item while we were syncing the gmw, let's reset the last budget item received time.
            // reason: if we received for example a single proposal +50 seconds ago, then once the budget sync starts (right after this call),
            // it will look like the sync is finished, and will not wait to receive any budget data and declare the sync over.
            g_tiertwo_sync_state.ResetLastBudgetItem();
            return false;
        }

        // timeout
        if (lastGamemasterWinner == 0 &&
            (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 2 || GetTime() - nAssetSyncStarted > GAMEMASTER_SYNC_TIMEOUT * 5)) {
            if (sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT)) {
                syncTimeout("GAMEMASTER_SYNC_GMW");
            } else {
                SwitchToNextAsset();
                // Same as above (future: remove all of this duplicated code in v6.0.)
                // in case we received a budget item while we were syncing the gmw, let's reset the last budget item received time.
                // reason: if we received for example a single proposal +50 seconds ago, then once the budget sync starts (right after this call),
                // it will look like the sync is finished, and will not wait to receive any budget data and declare the sync over.
                g_tiertwo_sync_state.ResetLastBudgetItem();
            }
            return false;
        }

        // Don't request gmw initial sync to more than 4 randomly ordered peers in this round.
        if (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 2) return false;

        // Request gmw sync if we haven't requested it yet.
        if (g_netfulfilledman.HasFulfilledRequest(pnode->addr, "gmwsync")) return true;

        // Mark sync requested.
        g_netfulfilledman.AddFulfilledRequest(pnode->addr, "gmwsync");

        // Sync gm winners
        int nGmCount = gamemasterman.CountEnabled(fLegacyGmObsolete /* only_legacy */);
        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETGMWINNERS, nGmCount));
        RequestedGamemasterAttempt++;

        return false; // sleep 1 second before do another request round.
    }

    if (RequestedGamemasterAssets == GAMEMASTER_SYNC_BUDGET) {
        int lastBudgetItem = g_tiertwo_sync_state.GetlastBudgetItem();
        // We'll start rejecting votes if we accidentally get set as synced too soon
        if (lastBudgetItem > 0 && lastBudgetItem < GetTime() - GAMEMASTER_SYNC_TIMEOUT * 10 && RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD) {
            // Hasn't received a new item in the last fifty seconds and more than GAMEMASTER_SYNC_THRESHOLD requests were sent,
            // so we'll move to the next asset
            SwitchToNextAsset();

            // Try to activate our gamemaster if possible
            activeGamemaster.ManageStatus();
            return false;
        }

        // timeout
        if (lastBudgetItem == 0 &&
            (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 3 || GetTime() - nAssetSyncStarted > GAMEMASTER_SYNC_TIMEOUT * 5)) {
            // maybe there is no budgets at all, so just finish syncing
            SwitchToNextAsset();
            activeGamemaster.ManageStatus();
            return false;
        }

        // Don't request budget initial sync to more than 6 randomly ordered peers in this round.
        if (RequestedGamemasterAttempt >= GAMEMASTER_SYNC_THRESHOLD * 3) return false;

        // Request bud sync if we haven't requested it yet.
        if (g_netfulfilledman.HasFulfilledRequest(pnode->addr, "busync")) return true;

        // Mark sync requested.
        g_netfulfilledman.AddFulfilledRequest(pnode->addr, "busync");

        // Sync proposals, finalizations and votes
        uint256 n;
        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::BUDGETVOTESYNC, n));
        RequestedGamemasterAttempt++;

        return false; // sleep 1 second before do another request round.
    }

    return true;
}
