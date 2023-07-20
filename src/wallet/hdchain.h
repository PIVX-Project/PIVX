// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_HDCHAIN_H
#define hemis_HDCHAIN_H

#include "key.h"

namespace HDChain {
    namespace ChangeType {
        static const uint8_t EXTERNAL = 0;
        static const uint8_t INTERNAL = 1;
        static const uint8_t STAKING = 2;
    };

    namespace ChainCounterType {
        static const uint8_t Standard  = 0;
        static const uint8_t Sapling   = 1;
    };
}

/* Simple HD chain data model for regular and sapling addresses */
class CHDChain
{
private:
    int nVersion;
    CKeyID seed_id;

public:
    // Standard/Sapling hd chain
    static const int CURRENT_VERSION = 2;
    // Single account counters.
    uint32_t nExternalChainCounter{0};
    uint32_t nInternalChainCounter{0};
    uint32_t nStakingChainCounter{0};
    // Chain counter type
    uint8_t chainType{HDChain::ChainCounterType::Standard};

    CHDChain(const uint8_t& _chainType = HDChain::ChainCounterType::Standard) : chainType(_chainType) { SetNull(); }

    SERIALIZE_METHODS(CHDChain, obj)
    {
        READWRITE(obj.nVersion, obj.seed_id, obj.nExternalChainCounter, obj.nInternalChainCounter, obj.nStakingChainCounter);
        if (obj.nVersion > 1) READWRITE(obj.chainType);
    }

    bool SetNull();
    bool IsNull() const;

    bool SetSeed(const CKeyID& seedId);
    CKeyID GetID() const { return seed_id; }

    uint32_t& GetChainCounter(const uint8_t& type = HDChain::ChangeType::EXTERNAL) {
        switch (type) {
            case HDChain::ChangeType::EXTERNAL:
                return nExternalChainCounter;
            case HDChain::ChangeType::INTERNAL:
                return nInternalChainCounter;
            case HDChain::ChangeType::STAKING:
                return nStakingChainCounter;
            default:
                throw std::runtime_error("HD chain type doesn't exist.");
        }
    }
};

#endif // hemis_HDCHAIN_H
