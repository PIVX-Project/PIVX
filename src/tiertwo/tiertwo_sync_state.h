// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_TIERTWO_SYNC_STATE_H
#define hemis_TIERTWO_SYNC_STATE_H

#include <atomic>
#include <map>

#define GAMEMASTER_SYNC_INITIAL 0
#define GAMEMASTER_SYNC_SPORKS 1
#define GAMEMASTER_SYNC_LIST 2
#define GAMEMASTER_SYNC_GMW 3
#define GAMEMASTER_SYNC_BUDGET 4
#define GAMEMASTER_SYNC_BUDGET_PROP 10
#define GAMEMASTER_SYNC_BUDGET_FIN 11
#define GAMEMASTER_SYNC_FAILED 998
#define GAMEMASTER_SYNC_FINISHED 999

// Sync threshold
#define GAMEMASTER_SYNC_THRESHOLD 2

// Chain sync update window.
// Be careful with this value. The smaller the value is, the more the tiertwo sync locks 'g_best_block_mutex'.
#define CHAIN_SYNC_UPDATE_TIME 30

class uint256;

class TierTwoSyncState {
public:
    bool IsBlockchainSynced() const { return fBlockchainSynced; };
    bool IsSynced() const { return m_current_sync_phase == GAMEMASTER_SYNC_FINISHED; }
    bool IsSporkListSynced() const { return m_current_sync_phase > GAMEMASTER_SYNC_SPORKS; }
    bool IsGamemasterListSynced() const { return m_current_sync_phase > GAMEMASTER_SYNC_LIST; }

    // Update seen maps
    void AddedGamemasterList(const uint256& hash);
    void AddedGamemasterWinner(const uint256& hash);
    void AddedBudgetItem(const uint256& hash);

    int64_t GetlastGamemasterList() const { return lastGamemasterList; }
    int64_t GetlastGamemasterWinner() const { return lastGamemasterWinner; }
    int64_t GetlastBudgetItem() const { return lastBudgetItem; }

    void ResetLastBudgetItem() { lastBudgetItem = 0; }

    void EraseSeenGMB(const uint256& hash) { mapSeenSyncGMB.erase(hash); }
    void EraseSeenGMW(const uint256& hash) { mapSeenSyncGMW.erase(hash); }
    void EraseSeenSyncBudget(const uint256& hash) { mapSeenSyncBudget.erase(hash); }

    // Reset seen data
    void ResetData();

    // Only called from gamemastersync and unit tests.
    void SetBlockchainSync(bool f, int64_t cur_time) {
        fBlockchainSynced = f;
        last_blockchain_sync_update_time = cur_time;
    };
    void SetCurrentSyncPhase(int sync_phase) { m_current_sync_phase = sync_phase; };
    int GetSyncPhase() const { return m_current_sync_phase; }

    // True if the last chain sync update was more than CHAIN_SYNC_UPDATE_TIME seconds ago
    bool CanUpdateChainSync(int64_t cur_time) const { return cur_time > last_blockchain_sync_update_time + CHAIN_SYNC_UPDATE_TIME; }

private:
    std::atomic<bool> fBlockchainSynced{false};
    std::atomic<int64_t> last_blockchain_sync_update_time{0};
    std::atomic<int> m_current_sync_phase{0};

    // Seen elements
    std::map<uint256, int> mapSeenSyncGMB;
    std::map<uint256, int> mapSeenSyncGMW;
    std::map<uint256, int> mapSeenSyncBudget;
    // Last seen time
    int64_t lastGamemasterList{0};
    int64_t lastGamemasterWinner{0};
    int64_t lastBudgetItem{0};
};

extern TierTwoSyncState g_tiertwo_sync_state;

#endif //hemis_TIERTWO_SYNC_STATE_H
