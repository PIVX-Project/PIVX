// Copyright (c) 2019 The Zcash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "consensus/params.h"
#include "consensus/upgrades.h"
#include "util/system.h"

namespace Consensus {

bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const
{
    if (idx >= Consensus::MAX_NETWORK_UPGRADES)
        return error("%s: Upgrade index out of bounds: %d >= %d",
                __func__, idx, Consensus::MAX_NETWORK_UPGRADES);

    if (nHeight < 0)
        return error("%s: Requested state for upgrade %s at negative height %d",
                __func__, NetworkUpgradeInfo[idx].strName, nHeight);

    return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
}

Optional<LLMQParams> Params::GetLLMQParams(uint8_t llmqtype) const
{
    const auto it = llmqs.find((LLMQType)llmqtype);
    if (it == llmqs.end()) {
        return nullopt;
    }
    return Optional<LLMQParams>(it->second);
}

} // End consensus namespace
