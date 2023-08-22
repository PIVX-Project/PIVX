// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_GAMEMASTER_META_MANAGER_H
#define hemis_GAMEMASTER_META_MANAGER_H

#include "serialize.h"
#include "sync.h"
#include "uint256.h"

#include <memory>

static const std::string GM_META_CACHE_FILENAME = "gmmetacache.dat";
static const std::string GM_META_CACHE_FILE_ID = "magicGamemasterMetaCache";

// Holds extra (non-deterministic) information about gamemasters
// This is mostly local information, e.g. last connection attempt
class CGamemasterMetaInfo
{
    friend class CGamemasterMetaMan;
private:
    mutable Mutex cs;
    uint256 proTxHash;
    // Connection data
    int64_t lastOutboundAttempt{0};
    int64_t lastOutboundSuccess{0};

public:
    CGamemasterMetaInfo() = default;
    explicit CGamemasterMetaInfo(const uint256& _proTxHash) : proTxHash(_proTxHash) {}
    CGamemasterMetaInfo(const CGamemasterMetaInfo& ref) :
            proTxHash(ref.proTxHash),
            lastOutboundAttempt(ref.lastOutboundAttempt),
            lastOutboundSuccess(ref.lastOutboundSuccess) {}

    SERIALIZE_METHODS(CGamemasterMetaInfo, obj) {
        READWRITE(obj.proTxHash, obj.lastOutboundAttempt, obj.lastOutboundSuccess);
    }

    const uint256& GetProTxHash() const { return proTxHash; }
    void SetLastOutboundAttempt(int64_t t) { LOCK(cs); lastOutboundAttempt = t; }
    int64_t GetLastOutboundAttempt() const { LOCK(cs); return lastOutboundAttempt; }
    void SetLastOutboundSuccess(int64_t t) { LOCK(cs); lastOutboundSuccess = t; }
    int64_t GetLastOutboundSuccess() const { LOCK(cs); return lastOutboundSuccess; }
};

typedef std::shared_ptr<CGamemasterMetaInfo> CGamemasterMetaInfoPtr;

class CGamemasterMetaMan
{
private:
    static const std::string SERIALIZATION_VERSION_STRING;
    mutable RecursiveMutex cs_metaman;
    std::map<uint256, CGamemasterMetaInfoPtr> metaInfos;

public:
    // Return the stored metadata info from an specific GM
    CGamemasterMetaInfoPtr GetMetaInfo(const uint256& proTxHash, bool fCreate = true);
    void Clear();
    std::string ToString();

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        LOCK(cs_metaman);
        s << SERIALIZATION_VERSION_STRING;
        std::vector<CGamemasterMetaInfo> tmpMetaInfo;
        for (auto& p : metaInfos) {
            tmpMetaInfo.emplace_back(*p.second);
        }
        s << tmpMetaInfo;
    }

    template <typename Stream>
    inline void Unserialize(Stream& s) {
        Clear();
        LOCK(cs_metaman);
        std::string strVersion;
        s >> strVersion;
        if (strVersion != SERIALIZATION_VERSION_STRING) {
            return;
        }

        std::vector<CGamemasterMetaInfo> tmpMetaInfo;
        s >> tmpMetaInfo;
        for (auto& mm : tmpMetaInfo) {
            metaInfos.emplace(mm.GetProTxHash(), std::make_shared<CGamemasterMetaInfo>(std::move(mm)));
        }
    }
};

extern CGamemasterMetaMan g_mmetaman;

#endif //hemis_GAMEMASTER_META_MANAGER_H
