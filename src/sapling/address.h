// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef ZC_ADDRESS_H_
#define ZC_ADDRESS_H_

#include "optional.h"
#include "sapling/sapling.h"
#include "serialize.h"
#include "uint256.h"

#include <array>
#include <boost/variant.hpp>

namespace libzcash {
class InvalidEncoding {
public:
    friend bool operator==(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
    friend bool operator<(const InvalidEncoding &a, const InvalidEncoding &b) { return true; }
};

const size_t SerializedSaplingPaymentAddressSize = 43;
const size_t SerializedSaplingFullViewingKeySize = 96;
const size_t SerializedSaplingExpandedSpendingKeySize = 96;
const size_t SerializedSaplingSpendingKeySize = 32;

//! Sapling functions.
class SaplingPaymentAddress {
public:
    diversifier_t d = {{0}};
    uint256 pk_d{UINT256_ZERO};

    SaplingPaymentAddress() {}
    SaplingPaymentAddress(const diversifier_t& _d, const uint256& _pk_d) : d(_d), pk_d(_pk_d) { }

    SERIALIZE_METHODS(SaplingPaymentAddress, obj) { READWRITE(obj.d, obj.pk_d); }

    //! Get the 256-bit SHA256d hash of this payment address.
    uint256 GetHash() const;

    friend inline bool operator==(const SaplingPaymentAddress& a, const SaplingPaymentAddress& b) {
        return a.d == b.d && a.pk_d == b.pk_d;
    }
    friend inline bool operator<(const SaplingPaymentAddress& a, const SaplingPaymentAddress& b) {
        return (a.d < b.d ||
                (a.d == b.d && a.pk_d < b.pk_d));
    }
};

class SaplingIncomingViewingKey : public uint256 {
public:
    SaplingIncomingViewingKey() : uint256() { }
    SaplingIncomingViewingKey(uint256 ivk) : uint256(ivk) { }

    // Can pass in diversifier for Sapling addr
    Optional<SaplingPaymentAddress> address(diversifier_t d) const;
};

class SaplingFullViewingKey {
public:
    uint256 ak;
    uint256 nk;
    uint256 ovk;

    SaplingFullViewingKey() : ak(), nk(), ovk() { }
    SaplingFullViewingKey(uint256 ak, uint256 nk, uint256 ovk) : ak(ak), nk(nk), ovk(ovk) { }

    SERIALIZE_METHODS(SaplingFullViewingKey, obj) { READWRITE(obj.ak, obj.nk, obj.ovk); }

    //! Get the fingerprint of this full viewing key (as defined in ZIP 32).
    uint256 GetFingerprint() const;

    SaplingIncomingViewingKey in_viewing_key() const;
    bool is_valid() const;

    friend inline bool operator==(const SaplingFullViewingKey& a, const SaplingFullViewingKey& b) {
        return a.ak == b.ak && a.nk == b.nk && a.ovk == b.ovk;
    }
    friend inline bool operator<(const SaplingFullViewingKey& a, const SaplingFullViewingKey& b) {
        return (a.ak < b.ak ||
            (a.ak == b.ak && a.nk < b.nk) ||
            (a.ak == b.ak && a.nk == b.nk && a.ovk < b.ovk));
    }
};


class SaplingExpandedSpendingKey {
public:
    uint256 ask;
    uint256 nsk;
    uint256 ovk;

    SaplingExpandedSpendingKey() : ask(), nsk(), ovk() { }
    SaplingExpandedSpendingKey(uint256 ask, uint256 nsk, uint256 ovk) : ask(ask), nsk(nsk), ovk(ovk) { }

    SERIALIZE_METHODS(SaplingExpandedSpendingKey, obj) { READWRITE(obj.ask, obj.nsk, obj.ovk); }

    SaplingFullViewingKey full_viewing_key() const;
    bool IsNull() { return ask.IsNull() && nsk.IsNull() && ovk.IsNull(); }

    friend inline bool operator==(const SaplingExpandedSpendingKey& a, const SaplingExpandedSpendingKey& b) {
        return a.ask == b.ask && a.nsk == b.nsk && a.ovk == b.ovk;
    }
    friend inline bool operator<(const SaplingExpandedSpendingKey& a, const SaplingExpandedSpendingKey& b) {
        return (a.ask < b.ask ||
            (a.ask == b.ask && a.nsk < b.nsk) ||
            (a.ask == b.ask && a.nsk == b.nsk && a.ovk < b.ovk));
    }
};

class SaplingSpendingKey : public uint256 {
public:
    SaplingSpendingKey() : uint256() { }
    SaplingSpendingKey(uint256 sk) : uint256(sk) { }

    static SaplingSpendingKey random();

    SaplingExpandedSpendingKey expanded_spending_key() const;
    SaplingFullViewingKey full_viewing_key() const;

    // Can derive Sapling addr from default diversifier
    SaplingPaymentAddress default_address() const;
};

typedef boost::variant<InvalidEncoding, SaplingPaymentAddress> PaymentAddress;

}

/** Check whether a PaymentAddress is not an InvalidEncoding. */
bool IsValidPaymentAddress(const libzcash::PaymentAddress& zaddr);

#endif // ZC_ADDRESS_H_
