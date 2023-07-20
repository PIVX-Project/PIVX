// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef ZC_NOTE_H_
#define ZC_NOTE_H_

#include "optional.h"
#include "sapling/address.h"
#include "sapling/noteencryption.h"
#include "sapling/sapling.h"
#include "uint256.h"

#include <array>

namespace libzcash {

/**
 * Notes are main primitive of Sapling, similar to a a tree of commitments.
 */
class BaseNote {
protected:
    uint64_t value_{0};
public:
    BaseNote() {}
    BaseNote(uint64_t value) : value_(value) {};
    virtual ~BaseNote() {};

    inline uint64_t value() const { return value_; };
};

class SaplingNote : public BaseNote {
public:
    diversifier_t d = {{0}};
    uint256 pk_d{UINT256_ZERO};
    uint256 r{UINT256_ZERO};

    SaplingNote() {};
    SaplingNote(const diversifier_t& _d, const uint256& _pk_d, uint64_t value, const uint256& _r):
        BaseNote(value),
        d(_d),
        pk_d(_pk_d),
        r(_r)
    {}
    SaplingNote(const SaplingPaymentAddress& address, uint64_t value);
    virtual ~SaplingNote() {};

    Optional<uint256> cmu() const;
    Optional<uint256> nullifier(const SaplingFullViewingKey& vk, const uint64_t position) const;
};

class BaseNotePlaintext {
protected:
    uint64_t value_{0};
    std::array<unsigned char, ZC_MEMO_SIZE> memo_ = {{0}};
public:
    BaseNotePlaintext() {}
    BaseNotePlaintext(const BaseNote& note, const std::array<unsigned char, ZC_MEMO_SIZE>& memo):
        value_(note.value()),
        memo_(memo)
    {}
    virtual ~BaseNotePlaintext() {}

    inline uint64_t value() const { return value_; }
    inline const std::array<unsigned char, ZC_MEMO_SIZE> & memo() const { return memo_; }
};

typedef std::pair<SaplingEncCiphertext, SaplingNoteEncryption> SaplingNotePlaintextEncryptionResult;

class SaplingNotePlaintext : public BaseNotePlaintext {
public:
    diversifier_t d = {{0}};
    uint256 rcm{UINT256_ZERO};

    SaplingNotePlaintext() {}
    SaplingNotePlaintext(const SaplingNote& note, const std::array<unsigned char, ZC_MEMO_SIZE>& memo);
    virtual ~SaplingNotePlaintext() {}

    static Optional<SaplingNotePlaintext> decrypt(
        const SaplingEncCiphertext& ciphertext,
        const uint256& ivk,
        const uint256& epk,
        const uint256& cmu
    );

    static Optional<SaplingNotePlaintext> decrypt(
        const SaplingEncCiphertext& ciphertext,
        const uint256& epk,
        const uint256& esk,
        const uint256& pk_d,
        const uint256& cmu
    );

    Optional<SaplingNote> note(const SaplingIncomingViewingKey& ivk) const;

    SERIALIZE_METHODS(SaplingNotePlaintext, obj)
    {
        unsigned char leadingByte = 0x01;
        READWRITE(leadingByte);

        if (leadingByte != 0x01) {
            throw std::ios_base::failure("lead byte of SaplingNotePlaintext is not recognized");
        }

        READWRITE(obj.d);           // 11 bytes
        READWRITE(obj.value_);      // 8 bytes
        READWRITE(obj.rcm);         // 32 bytes
        READWRITE(obj.memo_);       // 512 bytes
    }

    Optional<SaplingNotePlaintextEncryptionResult> encrypt(const uint256& pk_d) const;
};

class SaplingOutgoingPlaintext
{
public:
    uint256 pk_d{UINT256_ZERO};
    uint256 esk{UINT256_ZERO};

    SaplingOutgoingPlaintext() {};
    SaplingOutgoingPlaintext(const uint256& _pk_d, const uint256& _esk) :
        pk_d(_pk_d),
        esk(_esk)
    {}

    SERIALIZE_METHODS(SaplingOutgoingPlaintext, obj)
    {
        READWRITE(obj.pk_d);        // 8 bytes
        READWRITE(obj.esk);         // 8 bytes
    }

    static Optional<SaplingOutgoingPlaintext> decrypt(
        const SaplingOutCiphertext& ciphertext,
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        const uint256& epk
    );

    SaplingOutCiphertext encrypt(
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        SaplingNoteEncryption& enc
    ) const;
};


}

#endif // ZC_NOTE_H_
