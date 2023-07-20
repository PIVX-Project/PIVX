// Copyright (c) 2018-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_ZIP32_H
#define hemis_ZIP32_H

#include "key.h"
#include "optional.h"
#include "sapling/address.h"
#include "serialize.h"
#include "support/allocators/zeroafterfree.h"
#include "uint256.h"

const uint32_t ZIP32_HARDENED_KEY_LIMIT = 0x80000000;
const size_t ZIP32_XFVK_SIZE = 169;
const size_t ZIP32_XSK_SIZE = 169;

class HDSeed {
private:
    CPrivKey seed;

public:
    HDSeed() {}
    HDSeed(const CPrivKey& seedIn) : seed(seedIn) {}

    static HDSeed Random(size_t len = 32);
    bool IsNull() const { return seed.empty(); };
    uint256 Fingerprint() const;
    CPrivKey RawSeed() const { return seed; }

    friend bool operator==(const HDSeed& a, const HDSeed& b)
    {
        return a.seed == b.seed;
    }

    friend bool operator!=(const HDSeed& a, const HDSeed& b)
    {
        return !(a == b);
    }
};

// This is not part of ZIP 32, but is here because it's linked to the seed (for now).
uint256 ovkForShieldingFromTaddr(HDSeed& seed);

namespace libzcash {

typedef blob88 diversifier_index_t;

struct SaplingExtendedFullViewingKey {
    uint8_t depth;
    uint32_t parentFVKTag;
    uint32_t childIndex;
    uint256 chaincode;
    libzcash::SaplingFullViewingKey fvk;
    uint256 dk;

    SERIALIZE_METHODS(SaplingExtendedFullViewingKey, obj) { READWRITE(obj.depth, obj.parentFVKTag, obj.childIndex, obj.chaincode, obj.fvk, obj.dk); }

    Optional<SaplingExtendedFullViewingKey> Derive(uint32_t i) const;

    // Returns the first index starting from j that generates a valid
    // payment address, along with the corresponding address. Returns
    // an error if the diversifier space is exhausted.
    Optional<std::pair<diversifier_index_t, libzcash::SaplingPaymentAddress>>
        Address(diversifier_index_t j) const;

    libzcash::SaplingPaymentAddress DefaultAddress() const;

    friend inline bool operator==(const SaplingExtendedFullViewingKey& a, const SaplingExtendedFullViewingKey& b) {
        return (
            a.depth == b.depth &&
            a.parentFVKTag == b.parentFVKTag &&
            a.childIndex == b.childIndex &&
            a.chaincode == b.chaincode &&
            a.fvk == b.fvk &&
            a.dk == b.dk);
    }
    friend inline bool operator<(const SaplingExtendedFullViewingKey& a, const SaplingExtendedFullViewingKey& b) {
        return (a.depth < b.depth ||
            (a.depth == b.depth && a.childIndex < b.childIndex) ||
            (a.depth == b.depth && a.childIndex == b.childIndex && a.fvk < b.fvk));
    }
};

struct SaplingExtendedSpendingKey {
    uint8_t depth;
    uint32_t parentFVKTag;
    uint32_t childIndex;
    uint256 chaincode;
    libzcash::SaplingExpandedSpendingKey expsk;
    uint256 dk;

    SERIALIZE_METHODS(SaplingExtendedSpendingKey, obj) { READWRITE(obj.depth, obj.parentFVKTag, obj.childIndex, obj.chaincode, obj.expsk, obj.dk); }

    static SaplingExtendedSpendingKey Master(const HDSeed& seed);

    SaplingExtendedSpendingKey Derive(uint32_t i) const;

    SaplingExtendedFullViewingKey ToXFVK() const;

    libzcash::SaplingPaymentAddress DefaultAddress() const;

    friend bool operator==(const SaplingExtendedSpendingKey& a, const SaplingExtendedSpendingKey& b)
    {
        return a.depth == b.depth &&
            a.parentFVKTag == b.parentFVKTag &&
            a.childIndex == b.childIndex &&
            a.chaincode == b.chaincode &&
            a.expsk == b.expsk &&
            a.dk == b.dk;
    }
};

typedef boost::variant<InvalidEncoding, SaplingExtendedSpendingKey> SpendingKey;
typedef boost::variant<InvalidEncoding, SaplingExtendedFullViewingKey> ViewingKey;

}

/** Check whether a SpendingKey is not an InvalidEncoding. */
bool IsValidSpendingKey(const libzcash::SpendingKey& zkey);

/** Check whether a ViewingKey is not an InvalidEncoding. */
bool IsValidViewingKey(const libzcash::ViewingKey& vk);

#endif // hemis_ZIP32_H
