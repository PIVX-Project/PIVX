// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORKID_H
#define SPORKID_H

/*
    Don't ever reuse these IDs for other sporks
    - This would result in old clients getting confused about which spork is for what
*/

enum SporkId : int32_t {
    SPORK_2_SWIFTTX                             = 10001,      // Deprecated in v4.3.99
    SPORK_3_SWIFTTX_BLOCK_FILTERING             = 10002,      // Deprecated in v4.3.99
    SPORK_5_MAX_VALUE                           = 10004,      // Deprecated in v5.2.99
    SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT      = 10007,
    SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT       = 10008,
    SPORK_13_ENABLE_SUPERBLOCKS                 = 10012,
    SPORK_14_NEW_PROTOCOL_ENFORCEMENT           = 10013,
    SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2         = 10014,
    SPORK_16_ZEROCOIN_MAINTENANCE_MODE          = 10015,       // Deprecated in 5.2.99
    SPORK_17_COLDSTAKING_ENFORCEMENT            = 10017,       // Deprecated in 4.3.99
    SPORK_18_ZEROCOIN_PUBLICSPEND_V4            = 10018,       // Deprecated in 5.2.99
    SPORK_19_COLDSTAKING_MAINTENANCE            = 10019,
    SPORK_20_SAPLING_MAINTENANCE                = 10020,
    SPORK_21_LEGACY_MNS_MAX_HEIGHT              = 10021,
    SPORK_22_LLMQ_DKG_MAINTENANCE               = 10022,
    SPORK_23_CHAINLOCKS_ENFORCEMENT             = 10023,

    SPORK_INVALID                               = -1
};

// Default values
struct CSporkDef
{
    CSporkDef(): sporkId(SPORK_INVALID), defaultValue(0) {}
    CSporkDef(SporkId id, int64_t val, std::string n): sporkId(id), defaultValue(val), name(n) {}
    SporkId sporkId;
    int64_t defaultValue;
    std::string name;
};

#endif
