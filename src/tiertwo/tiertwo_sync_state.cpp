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
        if (it->second < GAMEMASTER_SYNC_THRESHOLD) {
            last = GetTime();
            it->second++;
        }
    } else {
        last = GetTime();
        mapSeen.emplace(hash, 1);
    }
}

void TierTwoSyncState::AddedGamemasterList(const uint256& hash)
{
    UpdateLastTime(hash, lastGamemasterList, mapSeenSyncGMB);
}

void TierTwoSyncState::AddedGamemasterWinner(const uint256& hash)
{
    UpdateLastTime(hash, lastGamemasterWinner, mapSeenSyncGMW);
}

void TierTwoSyncState::AddedBudgetItem(const uint256& hash)
{
    UpdateLastTime(hash, lastBudgetItem, mapSeenSyncBudget);
}

void TierTwoSyncState::ResetData()
{
    lastGamemasterList = 0;
    lastGamemasterWinner = 0;
    lastBudgetItem = 0;
    mapSeenSyncGMB.clear();
    mapSeenSyncGMW.clear();
    mapSeenSyncBudget.clear();
}
