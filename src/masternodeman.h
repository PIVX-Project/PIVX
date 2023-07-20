// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODEMAN_H
#define MASTERNODEMAN_H

#include "activemasternode.h"
#include "cyclingvector.h"
#include "key.h"
#include "key_io.h"
#include "masternode.h"
#include "net.h"
#include "sync.h"
#include "util/system.h"

#define MASTERNODES_REQUEST_SECONDS (60 * 60) // One hour.

/** Maximum number of block hashes to cache */
static const unsigned int CACHED_BLOCK_HASHES = 200;

class CMasternodeMan;
class CActiveMasternode;

extern CMasternodeMan mnodeman;
extern CActiveMasternode activeMasternode;

void DumpMasternodes();

/** Access to the MN database (mncache.dat)
 */
class CMasternodeDB
{
private:
    fs::path pathMN;
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

    CMasternodeDB();
    bool Write(const CMasternodeMan& mnodemanToSave);
    ReadResult Read(CMasternodeMan& mnodemanToLoad);
};


class CMasternodeMan
{
private:
    // critical section to protect the inner data structures
    mutable RecursiveMutex cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable RecursiveMutex cs_process_message;

    // map to hold all MNs (indexed by collateral outpoint)
    std::map<COutPoint, MasternodeRef> mapMasternodes;
    // who's asked for the Masternode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForMasternodeList;
    // who we asked for the Masternode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForMasternodeList;
    // which Masternodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForMasternodeListEntry;

    // Memory Only. Updated in NewBlock (blocks arrive in order)
    std::atomic<int> nBestHeight;

    // Memory Only. Cache last block hashes. Used to verify mn pings and winners.
    CyclingVector<uint256> cvLastBlockHashes;

    // Return the banning score (0 if no ban score increase is needed).
    int ProcessMNBroadcast(CNode* pfrom, CMasternodeBroadcast& mnb);
    int ProcessMNPing(CNode* pfrom, CMasternodePing& mnp);
    int ProcessMessageInner(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    // Relay a MN
    void BroadcastInvMN(CMasternode* mn, CNode* pfrom);

    // Validation
    bool CheckInputs(CMasternodeBroadcast& mnb, int nChainHeight, int& nDoS);

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, CMasternodeBroadcast> mapSeenMasternodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CMasternodePing> mapSeenMasternodePing;

    // keep track of dsq count to prevent masternodes from gaming obfuscation queue
    // TODO: Remove this from serialization
    int64_t nDsqCount;

    SERIALIZE_METHODS(CMasternodeMan, obj)
    {
        LOCK(obj.cs);
        READWRITE(obj.mapMasternodes);
        READWRITE(obj.mAskedUsForMasternodeList);
        READWRITE(obj.mWeAskedForMasternodeList);
        READWRITE(obj.mWeAskedForMasternodeListEntry);
        READWRITE(obj.nDsqCount);

        READWRITE(obj.mapSeenMasternodeBroadcast);
        READWRITE(obj.mapSeenMasternodePing);
    }

    CMasternodeMan();

    /// Add an entry
    bool Add(CMasternode& mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode* pnode, const CTxIn& vin);

    /// Check all Masternodes and remove inactive. Return the total masternode count.
    int CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Masternode vector
    void Clear();

    void SetBestHeight(int height) { nBestHeight.store(height, std::memory_order_release); };
    int GetBestHeight() const { return nBestHeight.load(std::memory_order_acquire); }

    int CountEnabled(bool only_legacy = false) const;

    bool RequestMnList(CNode* pnode);

    /// Find an entry
    CMasternode* Find(const COutPoint& collateralOut);
    const CMasternode* Find(const COutPoint& collateralOut) const;
    CMasternode* Find(const CPubKey& pubKeyMasternode);

    /// Check all transactions in a block, for spent masternode collateral outpoints (marking them as spent)
    void CheckSpentCollaterals(const std::vector<CTransactionRef>& vtx);

    /// Find an entry in the masternode list that is next to be paid
    MasternodeRef GetNextMasternodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount, const CBlockIndex* pChainTip = nullptr) const;

    /// Get the winner for this block hash
    MasternodeRef GetCurrentMasterNode(const uint256& hash) const;

    /// vector of pairs <masternode winner, height>
    std::vector<std::pair<MasternodeRef, int>> GetMnScores(int nLast) const;

    // Retrieve the known masternodes ordered by scoring without checking them. (Only used for listmasternodes RPC call)
    std::vector<std::pair<int64_t, MasternodeRef>> GetMasternodeRanks(int nBlockHeight) const;
    int GetMasternodeRank(const CTxIn& vin, int64_t nBlockHeight) const;

    bool ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, int& dosScore);

    // Process GETMNLIST message, returning the banning score (if 0, no ban score increase is needed)
    int ProcessGetMNList(CNode* pfrom, CTxIn& vin);

    struct MNsInfo {
        // All the known MNs
        int total{0};
        // enabled MNs eligible for payments. Older than 8000 seconds.
        int stableSize{0};
        // MNs enabled.
        int enabledSize{0};

        // Networks
        int ipv4{0};
        int ipv6{0};
        int onion{0};
    };

    // Return an overall status of the MNs list
    CMasternodeMan::MNsInfo getMNsInfo() const;

    std::string ToString() const;

    void Remove(const COutPoint& collateralOut);

    /// Update masternode list and maps using provided CMasternodeBroadcast
    void UpdateMasternodeList(CMasternodeBroadcast& mnb);

    /// Get the time a masternode was last paid
    int64_t GetLastPaid(const MasternodeRef& mn, int count_enabled, const CBlockIndex* BlockReading) const;
    int64_t SecondsSincePayment(const MasternodeRef& mn, int count_enabled, const CBlockIndex* BlockReading) const;

    // Block hashes cycling vector management
    void CacheBlockHash(const CBlockIndex* pindex);
    void UncacheBlockHash(const CBlockIndex* pindex);
    uint256 GetHashAtHeight(int nHeight) const;
    bool IsWithinDepth(const uint256& nHash, int depth) const;
    uint256 GetBlockHashToPing() const { return GetHashAtHeight(GetBestHeight() - MNPING_DEPTH); }
    std::vector<uint256> GetCachedBlocks() const { return cvLastBlockHashes.GetCache(); }
};

void ThreadCheckMasternodes();

#endif
