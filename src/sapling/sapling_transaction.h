// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_SAPLING_TRANSACTION_H
#define hemis_SAPLING_TRANSACTION_H

#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "consensus/consensus.h"

#include "sapling/noteencryption.h"
#include "sapling/sapling.h"

#include <boost/variant.hpp>

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   149
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
 * A shielded input to a transaction. It contains data that describes a Spend transfer.
 */
class SpendDescription
{
public:
    typedef std::array<unsigned char, 64> spend_auth_sig_t;

    uint256 cv{UINT256_ZERO};              //!< A value commitment to the value of the input note.
    uint256 anchor{UINT256_ZERO};          //!< A Merkle root of the Sapling note commitment tree at some block height in the past.
    uint256 nullifier{UINT256_ZERO};       //!< The nullifier of the input note.
    uint256 rk{UINT256_ZERO};              //!< The randomized public key for spendAuthSig.
    libzcash::GrothProof zkproof = {{0}};  //!< A zero-knowledge proof using the spend circuit.
    spend_auth_sig_t spendAuthSig = {{0}}; //!< A signature authorizing this spend.

    SpendDescription() {}

    SERIALIZE_METHODS(SpendDescription, obj) { READWRITE(obj.cv, obj.anchor, obj.nullifier, obj.rk, obj.zkproof, obj.spendAuthSig); }

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
 * A shielded output to a transaction. It contains data that describes an Output transfer.
 */
class OutputDescription
{
public:
    uint256 cv{UINT256_ZERO};                             //!< A value commitment to the value of the output note.
    uint256 cmu{UINT256_ZERO};                            //!< The u-coordinate of the note commitment for the output note.
    uint256 ephemeralKey{UINT256_ZERO};                   //!< A Jubjub public key.
    libzcash::SaplingEncCiphertext encCiphertext = {{0}}; //!< A ciphertext component for the encrypted output note.
    libzcash::SaplingOutCiphertext outCiphertext = {{0}}; //!< A ciphertext component for the encrypted output note.
    libzcash::GrothProof zkproof = {{0}};                 //!< A zero-knowledge proof using the output circuit.

    OutputDescription() {}

    SERIALIZE_METHODS(OutputDescription, obj) { READWRITE(obj.cv, obj.cmu, obj.ephemeralKey, obj.encCiphertext, obj.outCiphertext, obj.zkproof); }

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
    std::vector<SpendDescription> vShieldedSpend;
    std::vector<OutputDescription> vShieldedOutput;
    binding_sig_t bindingSig = {{0}};

    SERIALIZE_METHODS(SaplingTxData, obj) { READWRITE(obj.valueBalance, obj.vShieldedSpend, obj.vShieldedOutput, obj.bindingSig); }

    explicit SaplingTxData() : valueBalance(0), vShieldedSpend(), vShieldedOutput() { }
    explicit SaplingTxData(const SaplingTxData& from) : valueBalance(from.valueBalance), vShieldedSpend(from.vShieldedSpend), vShieldedOutput(from.vShieldedOutput), bindingSig(from.bindingSig) {}

    bool hasBindingSig() const
    {
        return std::any_of(bindingSig.begin(), bindingSig.end(),
                          [](const unsigned char& c){ return c != 0; });
    }
};


#endif //hemis_SAPLING_TRANSACTION_H
