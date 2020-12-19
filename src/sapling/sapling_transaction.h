// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SAPLING_TRANSACTION_H
#define PIVX_SAPLING_TRANSACTION_H

#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "consensus/consensus.h"

#include "sapling/noteencryption.hpp"
#include "sapling/sapling.h"
#include "sapling/proof.hpp"

#include <boost/variant.hpp>

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

// These constants are defined in the protocol § 7.1:
// https://zips.z.cash/protocol/protocol.pdf#txnencoding
#define OUTPUTDESCRIPTION_SIZE  948
#define SPENDDESCRIPTION_SIZE   384
#define BINDINGSIG_SIZE          64

namespace libzcash {
    static constexpr size_t GROTH_PROOF_SIZE = (
            48 + // π_A
            96 + // π_B
            48); // π_C

    typedef std::array<unsigned char, GROTH_PROOF_SIZE> GrothProof;
}

/**
 * A shield input to a transaction. It contains data that describes a Spend transfer.
 */
class SpendDescription
{
public:
    typedef std::array<unsigned char, 64> spend_auth_sig_t;

    uint256 cv;                    //!< A value commitment to the value of the input note.
    uint256 anchor;                //!< A Merkle root of the Sapling note commitment tree at some block height in the past.
    uint256 nullifier;             //!< The nullifier of the input note.
    uint256 rk;                    //!< The randomized public key for spendAuthSig.
    libzcash::GrothProof zkproof;  //!< A zero-knowledge proof using the spend circuit.
    spend_auth_sig_t spendAuthSig; //!< A signature authorizing this spend.

    SpendDescription() { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(cv);
        READWRITE(anchor);
        READWRITE(nullifier);
        READWRITE(rk);
        READWRITE(zkproof);
        READWRITE(spendAuthSig);
    }

    friend bool operator==(const SpendDescription& a, const SpendDescription& b)
    {
        return (
                a.cv == b.cv &&
                a.anchor == b.anchor &&
                a.nullifier == b.nullifier &&
                a.rk == b.rk &&
                a.zkproof == b.zkproof &&
                a.spendAuthSig == b.spendAuthSig
        );
    }

    friend bool operator!=(const SpendDescription& a, const SpendDescription& b)
    {
        return !(a == b);
    }
};

/**
 * A shield output to a transaction. It contains data that describes an Output transfer.
 */
class OutputDescription
{
public:
    uint256 cv;                     //!< A value commitment to the value of the output note.
    uint256 cmu;                     //!< The u-coordinate of the note commitment for the output note.
    uint256 ephemeralKey;           //!< A Jubjub public key.
    libzcash::SaplingEncCiphertext encCiphertext; //!< A ciphertext component for the encrypted output note.
    libzcash::SaplingOutCiphertext outCiphertext; //!< A ciphertext component for the encrypted output note.
    libzcash::GrothProof zkproof;   //!< A zero-knowledge proof using the output circuit.

    OutputDescription() { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(cv);
        READWRITE(cmu);
        READWRITE(ephemeralKey);
        READWRITE(encCiphertext);
        READWRITE(outCiphertext);
        READWRITE(zkproof);
    }

    friend bool operator==(const OutputDescription& a, const OutputDescription& b)
    {
        return (
                a.cv == b.cv &&
                a.cmu == b.cmu &&
                a.ephemeralKey == b.ephemeralKey &&
                a.encCiphertext == b.encCiphertext &&
                a.outCiphertext == b.outCiphertext &&
                a.zkproof == b.zkproof
        );
    }

    friend bool operator!=(const OutputDescription& a, const OutputDescription& b)
    {
        return !(a == b);
    }
};

class SaplingTxData
{
public:
    typedef std::array<unsigned char, BINDINGSIG_SIZE> binding_sig_t;

    CAmount valueBalance{0};
    std::vector<SpendDescription> vShieldSpend;
    std::vector<OutputDescription> vShieldOutput;
    binding_sig_t bindingSig = {{0}};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(*const_cast<CAmount*>(&valueBalance));
        READWRITE(*const_cast<std::vector<SpendDescription>*>(&vShieldSpend));
        READWRITE(*const_cast<std::vector<OutputDescription>*>(&vShieldOutput));
        READWRITE(*const_cast<binding_sig_t*>(&bindingSig));
    }

    explicit SaplingTxData() : valueBalance(0), vShieldSpend(), vShieldOutput() { }
    explicit SaplingTxData(const SaplingTxData& from) : valueBalance(from.valueBalance), vShieldSpend(from.vShieldSpend), vShieldOutput(from.vShieldOutput), bindingSig(from.bindingSig) {}

    bool hasBindingSig() const
    {
        return std::any_of(bindingSig.begin(), bindingSig.end(),
                          [](const unsigned char& c){ return c != 0; });
    }
};


#endif //PIVX_SAPLING_TRANSACTION_H
