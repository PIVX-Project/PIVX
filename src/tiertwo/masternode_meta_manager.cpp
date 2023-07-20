// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/masternode_meta_manager.h"
#include <sstream>

CMasternodeMetaMan g_mmetaman;

const std::string CMasternodeMetaMan::SERIALIZATION_VERSION_STRING = "CMasternodeMetaMan-Version-2";

CMasternodeMetaInfoPtr CMasternodeMetaMan::GetMetaInfo(const uint256& proTxHash, bool fCreate)
{
    LOCK(cs_metaman);
    auto it = metaInfos.find(proTxHash);
    if (it != metaInfos.end()) {
        return it->second;
    }
    if (!fCreate) {
        return nullptr;
    }
    it = metaInfos.emplace(proTxHash, std::make_shared<CMasternodeMetaInfo>(proTxHash)).first;
    return it->second;
}


void CMasternodeMetaMan::Clear()
{
    LOCK(cs_metaman);
    metaInfos.clear();
}

std::string CMasternodeMetaMan::ToString()
{
    LOCK(cs_metaman);
    std::ostringstream info;

    info << "Masternodes: meta infos object count: " << (int)metaInfos.size();
    return info.str();
}