// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamemasterman.h"

#include "addrman.h"
#include "evo/deterministicgms.h"
#include "fs.h"
#include "gamemaster-payments.h"
#include "gamemaster-sync.h"
#include "gamemaster.h"
#include "messagesigner.h"
#include "netbase.h"
#include "netmessagemaker.h"
#include "shutdown.h"
#include "spork.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "validation.h"

#include <boost/thread/thread.hpp>

#define GM_WINNER_MINIMUM_AGE 8000    // Age in seconds. This should be > GAMEMASTER_REMOVAL_SECONDS to avoid misconfigured new nodes in the list.

/** Gamemaster manager */
CGamemasterMan gamemasterman;
/** Keep track of the active Gamemaster */
CActiveGamemaster activeGamemaster;

struct CompareScoreGM {
    template <typename T>
    bool operator()(const std::pair<int64_t, T>& t1,
        const std::pair<int64_t, T>& t2) const
    {
        return t1.first < t2.first;
    }
};

//
// CGamemasterDB
//

static const int GAMEMASTER_DB_VERSION_BIP155 = 2;

CGamemasterDB::CGamemasterDB()
{
    pathGM = GetDataDir() / "gmcache.dat";
    strMagicMessage = "GamemasterCache";
}

bool CGamemasterDB::Write(const CGamemasterMan& gamemastermanToSave)
{
    int64_t nStart = GetTimeMillis();
    const auto& params = Params();

    // serialize, checksum data up to that point, then append checksum
    // Always done in the latest format.
    CDataStream ssGamemasters(SER_DISK, CLIENT_VERSION | ADDRV2_FORMAT);
    ssGamemasters << GAMEMASTER_DB_VERSION_BIP155;
    ssGamemasters << strMagicMessage;                   // gamemaster cache file specific magic message
    ssGamemasters << params.MessageStart(); // network specific magic number
    ssGamemasters << gamemastermanToSave;
    uint256 hash = Hash(ssGamemasters.begin(), ssGamemasters.end());
    ssGamemasters << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathGM, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathGM.string());

    // Write and commit header, data
    try {
        fileout << ssGamemasters;
    } catch (const std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    //    FileCommit(fileout);
    fileout.fclose();

    LogPrint(BCLog::GAMEMASTER,"Written info to gmcache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint(BCLog::GAMEMASTER,"  %s\n", gamemastermanToSave.ToString());

    return true;
}

CGamemasterDB::ReadResult CGamemasterDB::Read(CGamemasterMan& gamemastermanToLoad)
{
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathGM, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathGM.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = fs::file_size(pathGM);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)vchData.data(), dataSize);
        filein >> hashIn;
    } catch (const std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    const auto& params = Params();
    // serialize, checksum data up to that point, then append checksum
    CDataStream ssGamemasters(vchData, SER_DISK,  CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssGamemasters.begin(), ssGamemasters.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    int version;
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header
        ssGamemasters >> version;
        ssGamemasters >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid gamemaster cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        std::vector<unsigned char> pchMsgTmp(4);
        ssGamemasters >> MakeSpan(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp.data(), params.MessageStart(), pchMsgTmp.size()) != 0) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }
        // de-serialize data into CGamemasterMan object.
        if (version == GAMEMASTER_DB_VERSION_BIP155) {
            OverrideStream<CDataStream> s(&ssGamemasters, ssGamemasters.GetType(), ssGamemasters.GetVersion() | ADDRV2_FORMAT);
            s >> gamemastermanToLoad;
        } else {
            // Old format
            ssGamemasters >> gamemastermanToLoad;
        }
    } catch (const std::exception& e) {
        gamemastermanToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint(BCLog::GAMEMASTER,"Loaded info from gmcache.dat (dbversion=%d) %dms\n", version, GetTimeMillis() - nStart);
    LogPrint(BCLog::GAMEMASTER,"  %s\n", gamemastermanToLoad.ToString());

    return Ok;
}

void DumpGamemasters()
{
    int64_t nStart = GetTimeMillis();

    CGamemasterDB gmdb;
    LogPrint(BCLog::GAMEMASTER,"Writing info to gmcache.dat...\n");
    gmdb.Write(gamemasterman);

    LogPrint(BCLog::GAMEMASTER,"Gamemaster dump finished  %dms\n", GetTimeMillis() - nStart);
}

CGamemasterMan::CGamemasterMan():
        cvLastBlockHashes(CACHED_BLOCK_HASHES, UINT256_ZERO),
        nDsqCount(0)
{}

bool CGamemasterMan::Add(CGamemaster& gm)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        return false;
    }

    if (deterministicGMManager->GetListAtChainTip().HasGMByCollateral(gm.vin.prevout)) {
        LogPrint(BCLog::GAMEMASTER, "ERROR: Not Adding Gamemaster %s as the collateral is already registered with a DGM\n",
                gm.vin.prevout.ToString());
        return false;
    }

    LOCK(cs);

    if (!gm.IsAvailableState())
        return false;

    const auto& it = mapGamemasters.find(gm.vin.prevout);
    if (it == mapGamemasters.end()) {
        LogPrint(BCLog::GAMEMASTER, "Adding new Gamemaster %s\n", gm.vin.prevout.ToString());
        mapGamemasters.emplace(gm.vin.prevout, std::make_shared<CGamemaster>(gm));
        LogPrint(BCLog::GAMEMASTER, "Gamemaster added. New total count: %d\n", mapGamemasters.size());
        return true;
    }

    return false;
}

void CGamemasterMan::AskForGM(CNode* pnode, const CTxIn& vin)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        return;
    }

    std::map<COutPoint, int64_t>::iterator i = mWeAskedForGamemasterListEntry.find(vin.prevout);
    if (i != mWeAskedForGamemasterListEntry.end()) {
        int64_t t = (*i).second;
        if (GetTime() < t) return; // we've asked recently
    }

    // ask for the gmb info once from the node that sent gmp

    LogPrint(BCLog::GAMEMASTER, "CGamemasterMan::AskForGM - Asking node for missing entry, vin: %s\n", vin.prevout.hash.ToString());
    g_connman->PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::GETGMLIST, vin));
    int64_t askAgain = GetTime() + GamemasterMinPingSeconds();
    mWeAskedForGamemasterListEntry[vin.prevout] = askAgain;
}

int CGamemasterMan::CheckAndRemove(bool forceExpiredRemoval)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        LogPrint(BCLog::GAMEMASTER, "Removing all legacy gm due to SPORK 21\n");
        Clear();
        return 0;
    }

    LOCK(cs);

    //remove inactive and outdated (or replaced by DGM)
    auto it = mapGamemasters.begin();
    while (it != mapGamemasters.end()) {
        GamemasterRef& gm = it->second;
        auto activeState = gm->GetActiveState();
        if (activeState == CGamemaster::GAMEMASTER_REMOVE ||
            activeState == CGamemaster::GAMEMASTER_VIN_SPENT ||
            (forceExpiredRemoval && activeState == CGamemaster::GAMEMASTER_EXPIRED) ||
            gm->protocolVersion < ActiveProtocol()) {
            LogPrint(BCLog::GAMEMASTER, "Removing inactive (legacy) Gamemaster %s\n", it->first.ToString());
            //erase all of the broadcasts we've seen from this vin
            // -- if we missed a few pings and the node was removed, this will allow is to get it back without them
            //    sending a brand new gmb
            std::map<uint256, CGamemasterBroadcast>::iterator it3 = mapSeenGamemasterBroadcast.begin();
            while (it3 != mapSeenGamemasterBroadcast.end()) {
                if (it3->second.vin.prevout == it->first) {
                    g_tiertwo_sync_state.EraseSeenGMB((*it3).first);
                    it3 = mapSeenGamemasterBroadcast.erase(it3);
                } else {
                    ++it3;
                }
            }

            // allow us to ask for this gamemaster again if we see another ping
            std::map<COutPoint, int64_t>::iterator it2 = mWeAskedForGamemasterListEntry.begin();
            while (it2 != mWeAskedForGamemasterListEntry.end()) {
                if (it2->first == it->first) {
                    it2 = mWeAskedForGamemasterListEntry.erase(it2);
                } else {
                    ++it2;
                }
            }

            // clean GM pings right away.
            auto itPing = mapSeenGamemasterPing.begin();
            while (itPing != mapSeenGamemasterPing.end()) {
                if (itPing->second.GetVin().prevout == it->first) {
                    itPing = mapSeenGamemasterPing.erase(itPing);
                } else {
                    ++itPing;
                }
            }

            it = mapGamemasters.erase(it);
            LogPrint(BCLog::GAMEMASTER, "Gamemaster removed.\n");
        } else {
            ++it;
        }
    }
    LogPrint(BCLog::GAMEMASTER, "New total gamemaster count: %d\n", mapGamemasters.size());

    // check who's asked for the Gamemaster list
    std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForGamemasterList.begin();
    while (it1 != mAskedUsForGamemasterList.end()) {
        if ((*it1).second < GetTime()) {
            it1 = mAskedUsForGamemasterList.erase(it1);
        } else {
            ++it1;
        }
    }

    // check who we asked for the Gamemaster list
    it1 = mWeAskedForGamemasterList.begin();
    while (it1 != mWeAskedForGamemasterList.end()) {
        if ((*it1).second < GetTime()) {
            it1 = mWeAskedForGamemasterList.erase(it1);
        } else {
            ++it1;
        }
    }

    // check which Gamemasters we've asked for
    std::map<COutPoint, int64_t>::iterator it2 = mWeAskedForGamemasterListEntry.begin();
    while (it2 != mWeAskedForGamemasterListEntry.end()) {
        if ((*it2).second < GetTime()) {
            it2 = mWeAskedForGamemasterListEntry.erase(it2);
        } else {
            ++it2;
        }
    }

    // remove expired mapSeenGamemasterBroadcast
    std::map<uint256, CGamemasterBroadcast>::iterator it3 = mapSeenGamemasterBroadcast.begin();
    while (it3 != mapSeenGamemasterBroadcast.end()) {
        if ((*it3).second.lastPing.sigTime < GetTime() - (GamemasterRemovalSeconds() * 2)) {
            g_tiertwo_sync_state.EraseSeenGMB((*it3).second.GetHash());
            it3 = mapSeenGamemasterBroadcast.erase(it3);
        } else {
            ++it3;
        }
    }

    // remove expired mapSeenGamemasterPing
    std::map<uint256, CGamemasterPing>::iterator it4 = mapSeenGamemasterPing.begin();
    while (it4 != mapSeenGamemasterPing.end()) {
        if ((*it4).second.sigTime < GetTime() - (GamemasterRemovalSeconds() * 2)) {
            it4 = mapSeenGamemasterPing.erase(it4);
        } else {
            ++it4;
        }
    }

    return mapGamemasters.size();
}

void CGamemasterMan::Clear()
{
    LOCK(cs);
    mapGamemasters.clear();
    mAskedUsForGamemasterList.clear();
    mWeAskedForGamemasterList.clear();
    mWeAskedForGamemasterListEntry.clear();
    mapSeenGamemasterBroadcast.clear();
    mapSeenGamemasterPing.clear();
    nDsqCount = 0;
}

static void CountNetwork(const CService& addr, int& ipv4, int& ipv6, int& onion)
{
    std::string strHost;
    int port;
    SplitHostPort(addr.ToString(), port, strHost);
    CNetAddr node;
    LookupHost(strHost, node, false);
    switch(node.GetNetwork()) {
        case NET_IPV4:
            ipv4++;
            break;
        case NET_IPV6:
            ipv6++;
            break;
        case NET_ONION:
            onion++;
            break;
        default:
            break;
    }
}

CGamemasterMan::GMsInfo CGamemasterMan::getGMsInfo() const
{
    GMsInfo info;
    int nMinProtocol = ActiveProtocol();
    bool spork_8_active = sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT);

    // legacy gamemasters
    {
        LOCK(cs);
        for (const auto& it : mapGamemasters) {
            const GamemasterRef& gm = it.second;
            info.total++;
            CountNetwork(gm->addr, info.ipv4, info.ipv6, info.onion);
            if (gm->protocolVersion < nMinProtocol || !gm->IsEnabled()) {
                continue;
            }
            info.enabledSize++;
            // Eligible for payments
            if (spork_8_active && (GetAdjustedTime() - gm->sigTime < GM_WINNER_MINIMUM_AGE)) {
                continue; // Skip gamemasters younger than (default) 8000 sec (MUST be > GAMEMASTER_REMOVAL_SECONDS)
            }
            info.stableSize++;
        }
    }

    // deterministic gamemasters
    if (deterministicGMManager->IsDIP3Enforced()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(false, [&](const CDeterministicGMCPtr& dgm) {
            info.total++;
            CountNetwork(dgm->pdgmState->addr, info.ipv4, info.ipv6, info.onion);
            if (!dgm->IsPoSeBanned()) {
                info.enabledSize++;
                info.stableSize++;
            }
        });
    }

    return info;
}

int CGamemasterMan::CountEnabled(bool only_legacy) const
{
    int count_enabled = 0;
    int protocolVersion = ActiveProtocol();

    {
        LOCK(cs);
        for (const auto& it : mapGamemasters) {
            const GamemasterRef& gm = it.second;
            if (gm->protocolVersion < protocolVersion || !gm->IsEnabled()) continue;
            count_enabled++;
        }
    }

    if (!only_legacy && deterministicGMManager->IsDIP3Enforced()) {
        count_enabled += deterministicGMManager->GetListAtChainTip().GetValidGMsCount();
    }

    return count_enabled;
}

bool CGamemasterMan::RequestMnList(CNode* pnode)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        return false;
    }

    LOCK(cs);
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForGamemasterList.find(pnode->addr);
            if (it != mWeAskedForGamemasterList.end()) {
                if (GetTime() < (*it).second) {
                    LogPrint(BCLog::GAMEMASTER, "dseg - we already asked peer %i for the list; skipping...\n", pnode->GetId());
                    return false;
                }
            }
        }
    }

    g_connman->PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::GETGMLIST, CTxIn()));
    int64_t askAgain = GetTime() + GAMEMASTERS_REQUEST_SECONDS;
    mWeAskedForGamemasterList[pnode->addr] = askAgain;
    return true;
}

CGamemaster* CGamemasterMan::Find(const COutPoint& collateralOut)
{
    LOCK(cs);
    auto it = mapGamemasters.find(collateralOut);
    return it != mapGamemasters.end() ? it->second.get() : nullptr;
}

const CGamemaster* CGamemasterMan::Find(const COutPoint& collateralOut) const
{
    LOCK(cs);
    auto const& it = mapGamemasters.find(collateralOut);
    return it != mapGamemasters.end() ? it->second.get() : nullptr;
}

CGamemaster* CGamemasterMan::Find(const CPubKey& pubKeyGamemaster)
{
    LOCK(cs);

    for (auto& it : mapGamemasters) {
        GamemasterRef& gm = it.second;
        if (gm->pubKeyGamemaster == pubKeyGamemaster)
            return gm.get();
    }
    return nullptr;
}

void CGamemasterMan::CheckSpentCollaterals(const std::vector<CTransactionRef>& vtx)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        return;
    }

    LOCK(cs);
    for (const auto& tx : vtx) {
        for (const auto& in : tx->vin) {
            auto it = mapGamemasters.find(in.prevout);
            if (it != mapGamemasters.end()) {
                it->second->SetSpent();
            }
        }
    }
}

static bool canScheduleGM(bool fFilterSigTime, const GamemasterRef& gm, int minProtocol,
                          int nMnCount, int nBlockHeight)
{
    // check protocol version
    if (gm->protocolVersion < minProtocol) return false;

    // it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (gamemasterPayments.IsScheduled(*gm, nBlockHeight)) return false;

    // it's too new, wait for a cycle
    if (fFilterSigTime && gm->sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) return false;

    // make sure it has as many confirmations as there are gamemasters
    if (pcoinsTip->GetCoinDepthAtHeight(gm->vin.prevout, nBlockHeight) < nMnCount) return false;

    return true;
}

//
// Deterministically select the oldest/best gamemaster to pay on the network
//
GamemasterRef CGamemasterMan::GetNextGamemasterInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount, const CBlockIndex* pChainTip) const
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete(nBlockHeight)) {
        LogPrintf("%s: ERROR - called after legacy system disabled\n", __func__);
        return nullptr;
    }

    AssertLockNotHeld(cs_main);
    const CBlockIndex* BlockReading = (pChainTip == nullptr ? GetChainTip() : pChainTip);
    if (!BlockReading) return nullptr;

    GamemasterRef pBestGamemaster = nullptr;
    std::vector<std::pair<int64_t, GamemasterRef> > vecGamemasterLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    int minProtocol = ActiveProtocol();
    int count_enabled = CountEnabled();
    {
        LOCK(cs);
        for (const auto& it : mapGamemasters) {
            if (!it.second->IsEnabled()) continue;
            if (canScheduleGM(fFilterSigTime, it.second, minProtocol, count_enabled, nBlockHeight)) {
                vecGamemasterLastPaid.emplace_back(SecondsSincePayment(it.second, count_enabled, BlockReading), it.second);
            }
        }
    }
    // Add deterministic gamemasters to the vector
    if (deterministicGMManager->IsDIP3Enforced()) {
        CDeterministicGMList gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
            const GamemasterRef gm = MakeGamemasterRefForDGM(dgm);
            if (canScheduleGM(fFilterSigTime, gm, minProtocol, count_enabled, nBlockHeight)) {
                vecGamemasterLastPaid.emplace_back(SecondsSincePayment(gm, count_enabled, BlockReading), gm);
            }
        });
    }

    nCount = (int)vecGamemasterLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if (fFilterSigTime && nCount < count_enabled / 3) return GetNextGamemasterInQueueForPayment(nBlockHeight, false, nCount, BlockReading);

    // Sort them high to low
    sort(vecGamemasterLastPaid.rbegin(), vecGamemasterLastPaid.rend(), CompareScoreGM());

    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = count_enabled / 10;
    int nCountTenth = 0;
    arith_uint256 nHigh = ARITH_UINT256_ZERO;
    const uint256& hash = GetHashAtHeight(nBlockHeight - 101);
    for (const auto& s: vecGamemasterLastPaid) {
        const GamemasterRef pgm = s.second;
        if (!pgm) break;

        const arith_uint256& n = pgm->CalculateScore(hash);
        if (n > nHigh) {
            nHigh = n;
            pBestGamemaster = pgm;
        }
        nCountTenth++;
        if (nCountTenth >= nTenthNetwork) break;
    }
    return pBestGamemaster;
}

GamemasterRef CGamemasterMan::GetCurrentMasterNode(const uint256& hash) const
{
    int minProtocol = ActiveProtocol();
    int64_t score = 0;
    GamemasterRef winner = nullptr;

    // scan for winner
    for (const auto& it : mapGamemasters) {
        const GamemasterRef& gm = it.second;
        if (gm->protocolVersion < minProtocol || !gm->IsEnabled()) continue;
        // calculate the score of the gamemaster
        const int64_t n = gm->CalculateScore(hash).GetCompact(false);
        // determine the winner
        if (n > score) {
            score = n;
            winner = gm;
        }
    }

    // scan also dgms
    if (deterministicGMManager->IsDIP3Enforced()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
            const GamemasterRef gm = MakeGamemasterRefForDGM(dgm);
            // calculate the score of the gamemaster
            const int64_t n = gm->CalculateScore(hash).GetCompact(false);
            // determine the winner
            if (n > score) {
                score = n;
                winner = gm;
            }
        });
    }

    return winner;
}

std::vector<std::pair<GamemasterRef, int>> CGamemasterMan::GetMnScores(int nLast) const
{
    std::vector<std::pair<GamemasterRef, int>> ret;
    int nChainHeight = GetBestHeight();
    if (nChainHeight < 0) return ret;

    for (int nHeight = nChainHeight - nLast; nHeight < nChainHeight + 20; nHeight++) {
        const uint256& hash = GetHashAtHeight(nHeight - 101);
        GamemasterRef winner = GetCurrentMasterNode(hash);
        if (winner) {
            ret.emplace_back(winner, nHeight);
        }
    }
    return ret;
}

int CGamemasterMan::GetGamemasterRank(const CTxIn& vin, int64_t nBlockHeight) const
{
    const uint256& hash = GetHashAtHeight(nBlockHeight - 1);
    // height outside range
    if (hash == UINT256_ZERO) return -1;

    // scan for winner
    int minProtocol = ActiveProtocol();
    std::vector<std::pair<int64_t, CTxIn> > vecGamemasterScores;
    {
        LOCK(cs);
        for (const auto& it : mapGamemasters) {
            const GamemasterRef& gm = it.second;
            if (!gm->IsEnabled()) {
                continue; // Skip not enabled
            }
            if (gm->protocolVersion < minProtocol) {
                LogPrint(BCLog::GAMEMASTER,"Skipping Gamemaster with obsolete version %d\n", gm->protocolVersion);
                continue; // Skip obsolete versions
            }
            if (sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT) &&
                    GetAdjustedTime() - gm->sigTime < GM_WINNER_MINIMUM_AGE) {
                continue; // Skip gamemasters younger than (default) 1 hour
            }
            vecGamemasterScores.emplace_back(gm->CalculateScore(hash).GetCompact(false), gm->vin);
        }
    }

    // scan also dgms
    if (deterministicGMManager->IsDIP3Enforced()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
            const GamemasterRef gm = MakeGamemasterRefForDGM(dgm);
            vecGamemasterScores.emplace_back(gm->CalculateScore(hash).GetCompact(false), gm->vin);
        });
    }

    sort(vecGamemasterScores.rbegin(), vecGamemasterScores.rend(), CompareScoreGM());

    int rank = 0;
    for (std::pair<int64_t, CTxIn> & s : vecGamemasterScores) {
        rank++;
        if (s.second.prevout == vin.prevout) {
            return rank;
        }
    }

    return -1;
}

std::vector<std::pair<int64_t, GamemasterRef>> CGamemasterMan::GetGamemasterRanks(int nBlockHeight) const
{
    std::vector<std::pair<int64_t, GamemasterRef>> vecGamemasterScores;
    const uint256& hash = GetHashAtHeight(nBlockHeight - 1);
    // height outside range
    if (hash == UINT256_ZERO) return vecGamemasterScores;
    {
        LOCK(cs);
        // scan for winner
        for (const auto& it : mapGamemasters) {
            const GamemasterRef gm = it.second;
            const uint32_t score = gm->IsEnabled() ? gm->CalculateScore(hash).GetCompact(false) : 9999;

            vecGamemasterScores.emplace_back(score, gm);
        }
    }
    // scan also dgms
    if (deterministicGMManager->IsDIP3Enforced()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(false, [&](const CDeterministicGMCPtr& dgm) {
            const GamemasterRef gm = MakeGamemasterRefForDGM(dgm);
            const uint32_t score = dgm->IsPoSeBanned() ? 9999 : gm->CalculateScore(hash).GetCompact(false);

            vecGamemasterScores.emplace_back(score, gm);
        });
    }
    sort(vecGamemasterScores.rbegin(), vecGamemasterScores.rend(), CompareScoreGM());
    return vecGamemasterScores;
}

bool CGamemasterMan::CheckInputs(CGamemasterBroadcast& gmb, int nChainHeight, int& nDoS)
{
    const auto& consensus = Params().GetConsensus();
    // incorrect ping or its sigTime
    if(gmb.lastPing.IsNull() || !gmb.lastPing.CheckAndUpdate(nDoS, false, true)) {
        return false;
    }

    // search existing Gamemaster list
    CGamemaster* pgm = Find(gmb.vin.prevout);
    if (pgm != nullptr) {
        // nothing to do here if we already know about this gamemaster and it's enabled
        if (pgm->IsEnabled()) return true;
        // if it's not enabled, remove old GM first and continue
        else
            gamemasterman.Remove(pgm->vin.prevout);
    }

    const Coin& collateralUtxo = pcoinsTip->AccessCoin(gmb.vin.prevout);
    if (collateralUtxo.IsSpent()) {
        LogPrint(BCLog::GAMEMASTER,"gmb - vin %s spent\n", gmb.vin.prevout.ToString());
        return false;
    }

    // Check collateral value
    if (collateralUtxo.out.nValue != consensus.nGMCollateralAmt) {
        LogPrint(BCLog::GAMEMASTER,"gmb - invalid amount for gmb collateral %s\n", gmb.vin.prevout.ToString());
        nDoS = 33;
        return false;
    }

    // Check collateral association with gmb pubkey
    CScript payee = GetScriptForDestination(gmb.pubKeyCollateralAddress.GetID());
    if (collateralUtxo.out.scriptPubKey != payee) {
        LogPrint(BCLog::GAMEMASTER,"gmb - collateral %s not associated with gmb pubkey\n", gmb.vin.prevout.ToString());
        nDoS = 33;
        return false;
    }

    LogPrint(BCLog::GAMEMASTER, "gmb - Accepted Gamemaster entry\n");
    const int utxoHeight = (int) collateralUtxo.nHeight;
    int collateralUtxoDepth = nChainHeight - utxoHeight + 1;
    if (collateralUtxoDepth < consensus.GamemasterCollateralMinConf()) {
        LogPrint(BCLog::GAMEMASTER,"gmb - Input must have at least %d confirmations\n", consensus.GamemasterCollateralMinConf());
        // maybe we miss few blocks, let this gmb to be checked again later
        mapSeenGamemasterBroadcast.erase(gmb.GetHash());
        g_tiertwo_sync_state.EraseSeenGMB(gmb.GetHash());
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 HMS tx got GAMEMASTER_MIN_CONFIRMATIONS
    CBlockIndex* pConfIndex = WITH_LOCK(cs_main, return chainActive[utxoHeight + consensus.GamemasterCollateralMinConf() - 1]); // block where tx got GAMEMASTER_MIN_CONFIRMATIONS
    if (pConfIndex->GetBlockTime() > gmb.sigTime) {
        LogPrint(BCLog::GAMEMASTER,"gmb - Bad sigTime %d for Gamemaster %s (%i conf block is at %d)\n",
                 gmb.sigTime, gmb.vin.prevout.hash.ToString(), consensus.GamemasterCollateralMinConf(), pConfIndex->GetBlockTime());
        return false;
    }

    // Good input
    return true;
}

int CGamemasterMan::ProcessGMBroadcast(CNode* pfrom, CGamemasterBroadcast& gmb)
{
    const uint256& gmbHash = gmb.GetHash();
    if (mapSeenGamemasterBroadcast.count(gmbHash)) { //seen
        g_tiertwo_sync_state.AddedGamemasterList(gmbHash);
        return 0;
    }

    int chainHeight = GetBestHeight();
    int nDoS = 0;
    if (!gmb.CheckAndUpdate(nDoS)) {
        return nDoS;
    }

    // make sure it's still unspent
    if (!CheckInputs(gmb, chainHeight, nDoS)) {
        return nDoS; // error set internally
    }

    // now that did the gmb checks, can add it.
    mapSeenGamemasterBroadcast.emplace(gmbHash, gmb);

    // All checks performed, add it
    LogPrint(BCLog::GAMEMASTER,"%s - Got NEW Gamemaster entry - %s - %lli \n", __func__,
             gmb.vin.prevout.hash.ToString(), gmb.sigTime);
    CGamemaster gm(gmb);
    if (!Add(gm)) {
        LogPrint(BCLog::GAMEMASTER, "%s - Rejected Gamemaster entry %s\n", __func__,
                 gmb.vin.prevout.hash.ToString());
        return 0;
    }

    // if it matches our GM pubkey, then we've been remotely activated
    if (gmb.pubKeyGamemaster == activeGamemaster.pubKeyGamemaster && gmb.protocolVersion == PROTOCOL_VERSION) {
        activeGamemaster.EnableHotColdMasterNode(gmb.vin, gmb.addr);
    }

    // Relay only if we are synchronized and if the gmb address is not local.
    // Makes no sense to relay GMBs to the peers from where we are syncing them.
    bool isLocal = (gmb.addr.IsRFC1918() || gmb.addr.IsLocal()) && !Params().IsRegTestNet();
    if (!isLocal && g_tiertwo_sync_state.IsSynced()) gmb.Relay();

    // Add it as a peer
    g_connman->AddNewAddress(CAddress(gmb.addr, NODE_NETWORK), pfrom->addr, 2 * 60 * 60);

    // Update sync status
    g_tiertwo_sync_state.AddedGamemasterList(gmbHash);

    // All good
    return 0;
}

int CGamemasterMan::ProcessGMPing(CNode* pfrom, CGamemasterPing& gmp)
{
    const uint256& gmpHash = gmp.GetHash();
    if (mapSeenGamemasterPing.count(gmpHash)) return 0; //seen

    int nDoS = 0;
    if (gmp.CheckAndUpdate(nDoS)) return 0;

    if (nDoS > 0) {
        // if anything significant failed, mark that node
        return nDoS;
    } else {
        // if nothing significant failed, search existing Gamemaster list
        CGamemaster* pgm = Find(gmp.vin.prevout);
        // if it's known, don't ask for the gmb, just return
        if (pgm != nullptr) return 0;
    }

    // something significant is broken or gm is unknown,
    // we might have to ask for the gm entry (while we aren't syncing).
    if (g_tiertwo_sync_state.IsSynced()) {
        AskForGM(pfrom, gmp.vin);
    }

    // All good
    return 0;
}

void CGamemasterMan::BroadcastInvGM(CGamemaster* gm, CNode* pfrom)
{
    CGamemasterBroadcast gmb = CGamemasterBroadcast(*gm);
    const uint256& hash = gmb.GetHash();
    pfrom->PushInventory(CInv(MSG_GAMEMASTER_ANNOUNCE, hash));

    // Add to mapSeenGamemasterBroadcast in case that isn't there for some reason.
    if (!mapSeenGamemasterBroadcast.count(hash)) mapSeenGamemasterBroadcast.emplace(hash, gmb);
}

int CGamemasterMan::ProcessGetGMList(CNode* pfrom, CTxIn& vin)
{
    // Single GM request
    if (!vin.IsNull()) {
        CGamemaster* gm = Find(vin.prevout);
        if (!gm || !gm->IsEnabled()) return 0; // Nothing to return.

        // Relay the GM.
        BroadcastInvGM(gm, pfrom);
        LogPrint(BCLog::GAMEMASTER, "dseg - Sent 1 Gamemaster entry to peer %i\n", pfrom->GetId());
        return 0;
    }

    // Check if the node asked for gm list sync before.
    bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());
    if (!isLocal) {
        auto itAskedUsGMList = mAskedUsForGamemasterList.find(pfrom->addr);
        if (itAskedUsGMList != mAskedUsForGamemasterList.end()) {
            int64_t t = (*itAskedUsGMList).second;
            if (GetTime() < t) {
                LogPrintf("CGamemasterMan::ProcessMessage() : dseg - peer already asked me for the list\n");
                return 20;
            }
        }
        int64_t askAgain = GetTime() + GAMEMASTERS_REQUEST_SECONDS;
        mAskedUsForGamemasterList[pfrom->addr] = askAgain;
    }

    int nInvCount = 0;
    {
        LOCK(cs);
        for (auto& it : mapGamemasters) {
            GamemasterRef& gm = it.second;
            if (gm->addr.IsRFC1918()) continue; //local network
            if (gm->IsEnabled()) {
                LogPrint(BCLog::GAMEMASTER, "dseg - Sending Gamemaster entry - %s \n", gm->vin.prevout.hash.ToString());
                BroadcastInvGM(gm.get(), pfrom);
                nInvCount++;
            }
        }
    }

    g_connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::SYNCSTATUSCOUNT, GAMEMASTER_SYNC_LIST, nInvCount));
    LogPrint(BCLog::GAMEMASTER, "dseg - Sent %d Gamemaster entries to peer %i\n", nInvCount, pfrom->GetId());

    // All good
    return 0;
}

bool CGamemasterMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, int& dosScore)
{
    dosScore = ProcessMessageInner(pfrom, strCommand, vRecv);
    return dosScore == 0;
}

int CGamemasterMan::ProcessMessageInner(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) return 0;

    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        LogPrint(BCLog::GAMEMASTER, "%s: skip obsolete message %s\n", __func__, strCommand);
        return 0;
    }

    LOCK(cs_process_message);

    if (strCommand == NetMsgType::GMBROADCAST) {
        CGamemasterBroadcast gmb;
        vRecv >> gmb;
        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(gmb.GetHash(), MSG_GAMEMASTER_ANNOUNCE);
        }
        return ProcessGMBroadcast(pfrom, gmb);

    } else if (strCommand == NetMsgType::GMBROADCAST2) {
        CGamemasterBroadcast gmb;
        OverrideStream<CDataStream> s(&vRecv, vRecv.GetType(), vRecv.GetVersion() | ADDRV2_FORMAT);
        s >> gmb;
        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(gmb.GetHash(), MSG_GAMEMASTER_ANNOUNCE);
        }

        // For now, let's not process gmb2 with pre-BIP155 node addr format.
        if (gmb.addr.IsAddrV1Compatible()) {
            LogPrint(BCLog::GAMEMASTER, "%s: gmb2 with pre-BIP155 node addr format rejected\n", __func__);
            return 30;
        }

        return ProcessGMBroadcast(pfrom, gmb);

    } else if (strCommand == NetMsgType::GMPING) {
        //Gamemaster Ping
        CGamemasterPing gmp;
        vRecv >> gmp;
        LogPrint(BCLog::GMPING, "gmp - Gamemaster ping, vin: %s\n", gmp.vin.prevout.hash.ToString());
        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(gmp.GetHash(), MSG_GAMEMASTER_PING);
        }
        return ProcessGMPing(pfrom, gmp);

    } else if (strCommand == NetMsgType::GETGMLIST) {
        //Get Gamemaster list or specific entry
        CTxIn vin;
        vRecv >> vin;
        return ProcessGetGMList(pfrom, vin);
    }
    // Nothing to report
    return 0;
}

void CGamemasterMan::Remove(const COutPoint& collateralOut)
{
    LOCK(cs);
    const auto it = mapGamemasters.find(collateralOut);
    if (it != mapGamemasters.end()) {
        mapGamemasters.erase(it);
    }
}

void CGamemasterMan::UpdateGamemasterList(CGamemasterBroadcast& gmb)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        return;
    }

    mapSeenGamemasterPing.emplace(gmb.lastPing.GetHash(), gmb.lastPing);
    mapSeenGamemasterBroadcast.emplace(gmb.GetHash(), gmb);
    g_tiertwo_sync_state.AddedGamemasterList(gmb.GetHash());

    LogPrint(BCLog::GAMEMASTER,"%s -- gamemaster=%s\n", __func__, gmb.vin.prevout.ToString());

    CGamemaster* pgm = Find(gmb.vin.prevout);
    if (pgm == nullptr) {
        CGamemaster gm(gmb);
        Add(gm);
    } else {
        pgm->UpdateFromNewBroadcast(gmb);
    }
}

int64_t CGamemasterMan::SecondsSincePayment(const GamemasterRef& gm, int count_enabled, const CBlockIndex* BlockReading) const
{
    int64_t sec = (GetAdjustedTime() - GetLastPaid(gm, count_enabled, BlockReading));
    int64_t month = 60 * 60 * 24 * 30;
    if (sec < month) return sec; //if it's less than 30 days, give seconds

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << gm->vin;
    ss << gm->sigTime;
    const arith_uint256& hash = UintToArith256(ss.GetHash());

    // return some deterministic value for unknown/unpaid but force it to be more than 30 days old
    return month + hash.GetCompact(false);
}

int64_t CGamemasterMan::GetLastPaid(const GamemasterRef& gm, int count_enabled, const CBlockIndex* BlockReading) const
{
    if (BlockReading == nullptr) return false;

    const CScript& gmpayee = gm->GetPayeeScript();

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << gm->vin;
    ss << gm->sigTime;
    const uint256& hash = ss.GetHash();

    // use a deterministic offset to break a tie -- 2.5 minutes
    int64_t nOffset = UintToArith256(hash).GetCompact(false) % 150;

    int max_depth = count_enabled * 1.25;
    for (int n = 0; n < max_depth; n++) {
        const auto& it = gamemasterPayments.mapGamemasterBlocks.find(BlockReading->nHeight);
        if (it != gamemasterPayments.mapGamemasterBlocks.end()) {
            // Search for this payee, with at least 2 votes. This will aid in consensus
            // allowing the network to converge on the same payees quickly, then keep the same schedule.
            if (it->second.HasPayeeWithVotes(gmpayee, 2))
                return BlockReading->nTime + nOffset;
        }
        BlockReading = BlockReading->pprev;

        if (BlockReading == nullptr || BlockReading->nHeight <= 0) {
            break;
        }
    }

    return 0;
}

std::string CGamemasterMan::ToString() const
{
    std::ostringstream info;
    info << "Gamemasters: " << (int)mapGamemasters.size()
         << ", peers who asked us for Gamemaster list: " << (int)mAskedUsForGamemasterList.size()
         << ", peers we asked for Gamemaster list: " << (int)mWeAskedForGamemasterList.size()
         << ", entries in Gamemaster list we asked for: " << (int)mWeAskedForGamemasterListEntry.size();
    return info.str();
}

void CGamemasterMan::CacheBlockHash(const CBlockIndex* pindex)
{
    cvLastBlockHashes.Set(pindex->nHeight, pindex->GetBlockHash());
}

void CGamemasterMan::UncacheBlockHash(const CBlockIndex* pindex)
{
    cvLastBlockHashes.Set(pindex->nHeight, UINT256_ZERO);
}

uint256 CGamemasterMan::GetHashAtHeight(int nHeight) const
{
    // return zero if outside bounds
    if (nHeight < 0) {
        LogPrint(BCLog::GAMEMASTER, "%s: Negative height. Returning 0\n",  __func__);
        return UINT256_ZERO;
    }
    int nCurrentHeight = GetBestHeight();
    if (nHeight > nCurrentHeight) {
        LogPrint(BCLog::GAMEMASTER, "%s: height %d over current height %d. Returning 0\n",
                __func__, nHeight, nCurrentHeight);
        return UINT256_ZERO;
    }

    if (nHeight > nCurrentHeight - (int) CACHED_BLOCK_HASHES) {
        // Use cached hash
        return cvLastBlockHashes.Get(nHeight);
    } else {
        // Use chainActive
        LOCK(cs_main);
        return chainActive[nHeight]->GetBlockHash();
    }
}

bool CGamemasterMan::IsWithinDepth(const uint256& nHash, int depth) const
{
    // Sanity checks
    if (nHash.IsNull()) {
        return error("%s: Called with null hash\n", __func__);
    }
    if (depth < 0 || (unsigned) depth >= CACHED_BLOCK_HASHES) {
        return error("%s: Invalid depth %d. Cached block hashes: %d\n", __func__, depth, CACHED_BLOCK_HASHES);
    }
    // Check last depth blocks to find one with matching hash
    const int nCurrentHeight = GetBestHeight();
    int nStopHeight = std::max(0, nCurrentHeight - depth);
    for (int i = nCurrentHeight; i >= nStopHeight; i--) {
        if (GetHashAtHeight(i) == nHash)
            return true;
    }
    return false;
}

void ThreadCheckGamemasters()
{
    // Make this thread recognisable as the wallet flushing thread
    util::ThreadRename("hemis-gamemasterman");
    LogPrintf("Gamemasters thread started\n");

    unsigned int c = 0;

    try {
        // first clean up stale gamemaster payments data
        gamemasterPayments.CleanPaymentList(gamemasterman.CheckAndRemove(), gamemasterman.GetBestHeight());

        // Startup-only, clean any stored seen GM broadcast with an invalid service that
        // could have been invalidly stored on a previous release
        auto itSeenGMB = gamemasterman.mapSeenGamemasterBroadcast.begin();
        while (itSeenGMB != gamemasterman.mapSeenGamemasterBroadcast.end()) {
            if (!itSeenGMB->second.addr.IsValid()) {
                itSeenGMB = gamemasterman.mapSeenGamemasterBroadcast.erase(itSeenGMB);
            } else {
                itSeenGMB++;
            }
        }

        while (true) {

            if (ShutdownRequested()) {
                break;
            }

            MilliSleep(1000);
            boost::this_thread::interruption_point();

            // try to sync from all available nodes, one step at a time
            gamemasterSync.Process();

            if (g_tiertwo_sync_state.IsBlockchainSynced()) {
                c++;

                // check if we should activate or ping every few minutes,
                // start right after sync is considered to be done
                if (c % (GamemasterPingSeconds()/2) == 0)
                    activeGamemaster.ManageStatus();

                if (c % (GamemasterPingSeconds()/5) == 0) {
                    gamemasterPayments.CleanPaymentList(gamemasterman.CheckAndRemove(), gamemasterman.GetBestHeight());
                }
            }
        }
    } catch (boost::thread_interrupted&) {
        // nothing, thread interrupted.
    }
}
