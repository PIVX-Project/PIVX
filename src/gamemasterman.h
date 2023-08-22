// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMEMASTERMAN_H
#define GAMEMASTERMAN_H

#include "activegamemaster.h"
#include "cyclingvector.h"
#include "key.h"
#include "key_io.h"
#include "gamemaster.h"
#include "net.h"
#include "sync.h"
#include "util/system.h"

#define GAMEMASTERS_REQUEST_SECONDS (60 * 60) // One hour.

/** Maximum number of block hashes to cache */
static const unsigned int CACHED_BLOCK_HASHES = 200;

class CGamemasterMan;
class CActiveGamemaster;

extern CGamemasterMan gamemasterman;
extern CActiveGamemaster activeGamemaster;

void DumpGamemasters();

/** Access to the GM database (gmcache.dat)
 */
class CGamemasterDB
{
private:
    fs::path pathGM;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CGamemasterDB();
    bool Write(const CGamemasterMan& gamemastermanToSave);
    ReadResult Read(CGamemasterMan& gamemastermanToLoad);
};


class CGamemasterMan
{
private:
    // critical section to protect the inner data structures
    mutable RecursiveMutex cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable RecursiveMutex cs_process_message;

    // map to hold all GMs (indexed by collateral outpoint)
    std::map<COutPoint, GamemasterRef> mapGamemasters;
    // who's asked for the Gamemaster list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForGamemasterList;
    // who we asked for the Gamemaster list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForGamemasterList;
    // which Gamemasters we've asked for
    std::map<COutPoint, int64_t> mWeAskedForGamemasterListEntry;

    // Memory Only. Updated in NewBlock (blocks arrive in order)
    std::atomic<int> nBestHeight;

    // Memory Only. Cache last block hashes. Used to verify gm pings and winners.
    CyclingVector<uint256> cvLastBlockHashes;

    // Return the banning score (0 if no ban score increase is needed).
    int ProcessGMBroadcast(CNode* pfrom, CGamemasterBroadcast& gmb);
    int ProcessGMPing(CNode* pfrom, CGamemasterPing& gmp);
    int ProcessMessageInner(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    // Relay a GM
    void BroadcastInvGM(CGamemaster* gm, CNode* pfrom);

    // Validation
    bool CheckInputs(CGamemasterBroadcast& gmb, int nChainHeight, int& nDoS);

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, CGamemasterBroadcast> mapSeenGamemasterBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CGamemasterPing> mapSeenGamemasterPing;

    // keep track of dsq count to prevent gamemasters from gaming obfuscation queue
    // TODO: Remove this from serialization
    int64_t nDsqCount;

    SERIALIZE_METHODS(CGamemasterMan, obj)
    {
        LOCK(obj.cs);
        READWRITE(obj.mapGamemasters);
        READWRITE(obj.mAskedUsForGamemasterList);
        READWRITE(obj.mWeAskedForGamemasterList);
        READWRITE(obj.mWeAskedForGamemasterListEntry);
        READWRITE(obj.nDsqCount);

        READWRITE(obj.mapSeenGamemasterBroadcast);
        READWRITE(obj.mapSeenGamemasterPing);
    }

    CGamemasterMan();

    /// Add an entry
    bool Add(CGamemaster& gm);

    /// Ask (source) node for gmb
    void AskForGM(CNode* pnode, const CTxIn& vin);

    /// Check all Gamemasters and remove inactive. Return the total gamemaster count.
    int CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Gamemaster vector
    void Clear();

    void SetBestHeight(int height) { nBestHeight.store(height, std::memory_order_release); };
    int GetBestHeight() const { return nBestHeight.load(std::memory_order_acquire); }

    int CountEnabled(bool only_legacy = false) const;

    bool RequestGmList(CNode* pnode);

    /// Find an entry
    CGamemaster* Find(const COutPoint& collateralOut);
    const CGamemaster* Find(const COutPoint& collateralOut) const;
    CGamemaster* Find(const CPubKey& pubKeyGamemaster);

    /// Check all transactions in a block, for spent gamemaster collateral outpoints (marking them as spent)
    void CheckSpentCollaterals(const std::vector<CTransactionRef>& vtx);

    /// Find an entry in the gamemaster list that is next to be paid
    GamemasterRef GetNextGamemasterInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount, const CBlockIndex* pChainTip = nullptr) const;

    /// Get the winner for this block hash
    GamemasterRef GetCurrentMasterNode(const uint256& hash) const;

    /// vector of pairs <gamemaster winner, height>
    std::vector<std::pair<GamemasterRef, int>> GetGmScores(int nLast) const;

    // Retrieve the known gamemasters ordered by scoring without checking them. (Only used for listgamemasters RPC call)
    std::vector<std::pair<int64_t, GamemasterRef>> GetGamemasterRanks(int nBlockHeight) const;
    int GetGamemasterRank(const CTxIn& vin, int64_t nBlockHeight) const;

    bool ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, int& dosScore);

    // Process GETGMLIST message, returning the banning score (if 0, no ban score increase is needed)
    int ProcessGetGMList(CNode* pfrom, CTxIn& vin);

    struct GMsInfo {
        // All the known GMs
        int total{0};
        // enabled GMs eligible for payments. Older than 8000 seconds.
        int stableSize{0};
        // GMs enabled.
        int enabledSize{0};

        // Networks
        int ipv4{0};
        int ipv6{0};
        int onion{0};
    };

    // Return an overall status of the GMs list
    CGamemasterMan::GMsInfo getGMsInfo() const;

    std::string ToString() const;

    void Remove(const COutPoint& collateralOut);

    /// Update gamemaster list and maps using provided CGamemasterBroadcast
    void UpdateGamemasterList(CGamemasterBroadcast& gmb);

    /// Get the time a gamemaster was last paid
    int64_t GetLastPaid(const GamemasterRef& gm, int count_enabled, const CBlockIndex* BlockReading) const;
    int64_t SecondsSincePayment(const GamemasterRef& gm, int count_enabled, const CBlockIndex* BlockReading) const;

    // Block hashes cycling vector management
    void CacheBlockHash(const CBlockIndex* pindex);
    void UncacheBlockHash(const CBlockIndex* pindex);
    uint256 GetHashAtHeight(int nHeight) const;
    bool IsWithinDepth(const uint256& nHash, int depth) const;
    uint256 GetBlockHashToPing() const { return GetHashAtHeight(GetBestHeight() - GMPING_DEPTH); }
    std::vector<uint256> GetCachedBlocks() const { return cvLastBlockHashes.GetCache(); }
};

void ThreadCheckGamemasters();

#endif
