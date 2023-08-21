// Copyright (c) 2018 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_SPENDTYPE_H
#define hemis_SPENDTYPE_H

#include <cstdint>

namespace libzerocoin {
    enum SpendType : uint8_t {
        SPEND, // Used for a typical spend transaction, zHMS should be unusable after
        STAKE, // Used for a spend that occurs as a stake
        GM_COLLATERAL, // Used when proving ownership of zHMS that will be used for gamemasters (future)
        SIGN_MESSAGE // Used to sign messages that do not belong above (future)
    };
}

#endif //hemis_SPENDTYPE_H
