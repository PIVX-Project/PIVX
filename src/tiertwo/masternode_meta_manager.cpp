// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/gamemaster_meta_manager.h"
#include <sstream>

CGamemasterMetaMan g_mmetaman;

const std::string CGamemasterMetaMan::SERIALIZATION_VERSION_STRING = "CGamemasterMetaMan-Version-2";

CGamemasterMetaInfoPtr CGamemasterMetaMan::GetMetaInfo(const uint256& proTxHash, bool fCreate)
{
    LOCK(cs_metaman);
    auto it = metaInfos.find(proTxHash);
    if (it != metaInfos.end()) {
        return it->second;
    }
    if (!fCreate) {
        return nullptr;
    }
    it = metaInfos.emplace(proTxHash, std::make_shared<CGamemasterMetaInfo>(proTxHash)).first;
    return it->second;
}


void CGamemasterMetaMan::Clear()
{
    LOCK(cs_metaman);
    metaInfos.clear();
}

std::string CGamemasterMetaMan::ToString()
{
    LOCK(cs_metaman);
    std::ostringstream info;

    info << "Gamemasters: meta infos object count: " << (int)metaInfos.size();
    return info.str();
}