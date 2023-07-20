// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/tiertwo_sync_state.h"
#include "uint256.h"
#include "utiltime.h"

TierTwoSyncState g_tiertwo_sync_state;

static void UpdateLastTime(const uint256& hash, int64_t& last, std::map<uint256, int>& mapSeen)
{
    auto it = mapSeen.find(hash);
    if (it != mapSeen.end()) {
        if (it->second < MASTERNODE_SYNC_THRESHOLD) {
            last = GetTime();
            it->second++;
        }
    } else {
        last = GetTime();
        mapSeen.emplace(hash, 1);
    }
}

void TierTwoSyncState::AddedMasternodeList(const uint256& hash)
{
    UpdateLastTime(hash, lastMasternodeList, mapSeenSyncMNB);
}

void TierTwoSyncState::AddedMasternodeWinner(const uint256& hash)
{
    UpdateLastTime(hash, lastMasternodeWinner, mapSeenSyncMNW);
}

void TierTwoSyncState::AddedBudgetItem(const uint256& hash)
{
    UpdateLastTime(hash, lastBudgetItem, mapSeenSyncBudget);
}

void TierTwoSyncState::ResetData()
{
    lastMasternodeList = 0;
    lastMasternodeWinner = 0;
    lastBudgetItem = 0;
    mapSeenSyncMNB.clear();
    mapSeenSyncMNW.clear();
    mapSeenSyncBudget.clear();
}
