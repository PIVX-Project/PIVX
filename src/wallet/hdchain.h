//Copyright (c) 2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Sombe_HDCHAIN_H
#define Sombe_HDCHAIN_H

#include "key.h"

namespace HDChain {
    namespace ChangeType {
        static const uint8_t EXTERNAL = 0;
        static const uint8_t INTERNAL = 1;
        static const uint8_t STAKING = 2;
    };
}

/* Simple HD chain data model */
class CHDChain
{
private:
    int nVersion;
    CKeyID seed_id;

public:
    static const int CURRENT_VERSION = 1;
    // Single account counters.
    uint32_t nExternalChainCounter{0};
    uint32_t nInternalChainCounter{0};
    uint32_t nStakingChainCounter{0};

    CHDChain() { SetNull(); }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(seed_id);
        // Single account counters.
        READWRITE(nExternalChainCounter);
        READWRITE(nInternalChainCounter);
        READWRITE(nStakingChainCounter);
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

#endif // Sombe_HDCHAIN_H
