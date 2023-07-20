// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/hdchain.h"

#include "chainparams.h"
#include "key_io.h"
#include "tinyformat.h"
#include "util/system.h"
#include "utilstrencodings.h"

bool CHDChain::SetNull()
{
    nVersion = CURRENT_VERSION;
    seed_id = CKeyID();
    nExternalChainCounter = 0;
    nInternalChainCounter = 0;
    nStakingChainCounter = 0;
    return IsNull();
}

bool CHDChain::IsNull() const
{
    return seed_id.IsNull();
}

bool CHDChain::SetSeed( const CKeyID& seedId)
{
    // Cannot be initialized twice
    if (!IsNull())
        return false;

    seed_id = seedId;
    return true;
}