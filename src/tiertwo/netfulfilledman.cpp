// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/netfulfilledman.h"
#include "chainparams.h"
#include "netaddress.h"
#include "shutdown.h"
#include "utiltime.h"

CNetFulfilledRequestManager g_netfulfilledman(DEFAULT_ITEMS_FILTER_SIZE);

CNetFulfilledRequestManager::CNetFulfilledRequestManager(unsigned int _itemsFilterSize)
{
    itemsFilterSize = _itemsFilterSize;
    if (itemsFilterSize != 0) {
        itemsFilter = std::make_unique<CBloomFilter>(itemsFilterSize, 0.001, 0, BLOOM_UPDATE_ALL);
    }
}

void CNetFulfilledRequestManager::AddFulfilledRequest(const CService& addr, const std::string& strRequest)
{
    LOCK(cs_mapFulfilledRequests);
    mapFulfilledRequests[addr][strRequest] = GetTime() + Params().FulfilledRequestExpireTime();
}

bool CNetFulfilledRequestManager::HasFulfilledRequest(const CService& addr, const std::string& strRequest) const
{
    LOCK(cs_mapFulfilledRequests);
    auto it = mapFulfilledRequests.find(addr);
    if (it != mapFulfilledRequests.end()) {
        auto itReq = it->second.find(strRequest);
        if (itReq != it->second.end()) {
            return itReq->second > GetTime();
        }
    }
    return false;
}

static std::vector<unsigned char> convertElement(const CService& addr, const uint256& itemHash)
{
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << addr.GetAddrBytes();
    stream << itemHash;
    return {stream.begin(), stream.end()};
}

void CNetFulfilledRequestManager::AddItemRequest(const CService& addr, const uint256& itemHash)
{
    LOCK(cs_mapFulfilledRequests);
    assert(itemsFilter);
    itemsFilter->insert(convertElement(addr, itemHash));
    itemsFilterCount++;
}

bool CNetFulfilledRequestManager::HasItemRequest(const CService& addr, const uint256& itemHash) const
{
    LOCK(cs_mapFulfilledRequests);
    assert(itemsFilter);
    return itemsFilter->contains(convertElement(addr, itemHash));
}

void CNetFulfilledRequestManager::CheckAndRemove()
{
    LOCK(cs_mapFulfilledRequests);
    int64_t now = GetTime();
    for (auto it = mapFulfilledRequests.begin(); it != mapFulfilledRequests.end();) {
        for (auto it_entry = it->second.begin(); it_entry != it->second.end();) {
            if (now > it_entry->second) {
                it_entry = it->second.erase(it_entry);
            } else {
                it_entry++;
            }
        }
        if (it->second.empty()) {
            it = mapFulfilledRequests.erase(it);
        } else {
            it++;
        }
    }

    if (now > lastFilterCleanup ||  itemsFilterCount >= itemsFilterSize) {
        itemsFilter->clear();
        itemsFilterCount = 0;
        lastFilterCleanup = now + filterCleanupTime;
    }
}

void CNetFulfilledRequestManager::Clear()
{
    LOCK(cs_mapFulfilledRequests);
    mapFulfilledRequests.clear();
}

std::string CNetFulfilledRequestManager::ToString() const
{
    LOCK(cs_mapFulfilledRequests);
    std::ostringstream info;
    info << "Nodes with fulfilled requests: " << (int)mapFulfilledRequests.size();
    return info.str();
}

void CNetFulfilledRequestManager::DoMaintenance()
{
    if (ShutdownRequested()) return;
    CheckAndRemove();
}
