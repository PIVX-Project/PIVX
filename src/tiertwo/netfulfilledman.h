// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_NETFULFILLEDMAN_H
#define hemis_NETFULFILLEDMAN_H

#include "bloom.h"
#include "serialize.h"
#include "sync.h"

#include <map>

class CBloomFilter;
class CService;

static const std::string NET_REQUESTS_CACHE_FILENAME = "netrequests.dat";
static const std::string NET_REQUESTS_CACHE_FILE_ID = "magicNetRequestsCache";

static const unsigned int DEFAULT_ITEMS_FILTER_SIZE = 250;
static const unsigned int DEFAULT_ITEMS_FILTER_CLEANUP = 60 * 60;

// Fulfilled requests are used to prevent nodes from asking the same data on sync
// and being banned for doing it too often.
class CNetFulfilledRequestManager
{
private:
    typedef std::map<std::string, int64_t> fulfilledreqmapentry_t;
    typedef std::map<CService, fulfilledreqmapentry_t> fulfilledreqmap_t;

    // Keep track of what node has/was asked for and when
    fulfilledreqmap_t mapFulfilledRequests GUARDED_BY(cs_mapFulfilledRequests);
    mutable Mutex cs_mapFulfilledRequests;

    std::unique_ptr<CBloomFilter> itemsFilter GUARDED_BY(cs_mapFulfilledRequests){nullptr};
    unsigned int itemsFilterSize{0};
    unsigned int itemsFilterCount{0};
    int64_t filterCleanupTime{DEFAULT_ITEMS_FILTER_CLEANUP}; // for now, fixed cleanup time
    int64_t lastFilterCleanup{0};

public:
    CNetFulfilledRequestManager(unsigned int itemsFilterSize);

    SERIALIZE_METHODS(CNetFulfilledRequestManager, obj) {
        LOCK(obj.cs_mapFulfilledRequests);
        READWRITE(obj.mapFulfilledRequests);
    }

    void AddFulfilledRequest(const CService& addr, const std::string& strRequest);
    bool HasFulfilledRequest(const CService& addr, const std::string& strRequest) const;

    // Faster lookup using bloom filter
    void AddItemRequest(const CService& addr, const uint256& itemHash);
    bool HasItemRequest(const CService& addr, const uint256& itemHash) const;

    void CheckAndRemove();
    void Clear();

    std::string ToString() const;
    int Size() const { return WITH_LOCK(cs_mapFulfilledRequests, return mapFulfilledRequests.size();); }

    void DoMaintenance();
};

extern CNetFulfilledRequestManager g_netfulfilledman;

#endif // hemis_NETFULFILLEDMAN_H
