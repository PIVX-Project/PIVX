// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_MASTERNODE_META_MANAGER_H
#define hemis_MASTERNODE_META_MANAGER_H

#include "serialize.h"
#include "sync.h"
#include "uint256.h"

#include <memory>

static const std::string MN_META_CACHE_FILENAME = "mnmetacache.dat";
static const std::string MN_META_CACHE_FILE_ID = "magicMasternodeMetaCache";

// Holds extra (non-deterministic) information about masternodes
// This is mostly local information, e.g. last connection attempt
class CMasternodeMetaInfo
{
    friend class CMasternodeMetaMan;
private:
    mutable Mutex cs;
    uint256 proTxHash;
    // Connection data
    int64_t lastOutboundAttempt{0};
    int64_t lastOutboundSuccess{0};

public:
    CMasternodeMetaInfo() = default;
    explicit CMasternodeMetaInfo(const uint256& _proTxHash) : proTxHash(_proTxHash) {}
    CMasternodeMetaInfo(const CMasternodeMetaInfo& ref) :
            proTxHash(ref.proTxHash),
            lastOutboundAttempt(ref.lastOutboundAttempt),
            lastOutboundSuccess(ref.lastOutboundSuccess) {}

    SERIALIZE_METHODS(CMasternodeMetaInfo, obj) {
        READWRITE(obj.proTxHash, obj.lastOutboundAttempt, obj.lastOutboundSuccess);
    }

    const uint256& GetProTxHash() const { return proTxHash; }
    void SetLastOutboundAttempt(int64_t t) { LOCK(cs); lastOutboundAttempt = t; }
    int64_t GetLastOutboundAttempt() const { LOCK(cs); return lastOutboundAttempt; }
    void SetLastOutboundSuccess(int64_t t) { LOCK(cs); lastOutboundSuccess = t; }
    int64_t GetLastOutboundSuccess() const { LOCK(cs); return lastOutboundSuccess; }
};

typedef std::shared_ptr<CMasternodeMetaInfo> CMasternodeMetaInfoPtr;

class CMasternodeMetaMan
{
private:
    static const std::string SERIALIZATION_VERSION_STRING;
    mutable RecursiveMutex cs_metaman;
    std::map<uint256, CMasternodeMetaInfoPtr> metaInfos;

public:
    // Return the stored metadata info from an specific MN
    CMasternodeMetaInfoPtr GetMetaInfo(const uint256& proTxHash, bool fCreate = true);
    void Clear();
    std::string ToString();

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        LOCK(cs_metaman);
        s << SERIALIZATION_VERSION_STRING;
        std::vector<CMasternodeMetaInfo> tmpMetaInfo;
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

        std::vector<CMasternodeMetaInfo> tmpMetaInfo;
        s >> tmpMetaInfo;
        for (auto& mm : tmpMetaInfo) {
            metaInfos.emplace(mm.GetProTxHash(), std::make_shared<CMasternodeMetaInfo>(std::move(mm)));
        }
    }
};

extern CMasternodeMetaMan g_mmetaman;

#endif //hemis_MASTERNODE_META_MANAGER_H
