// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "sapling/note.h"

#include "crypto/sha256.h"
#include "random.h"
#include "sapling/prf.h"
#include "sapling/sapling_util.h"
#include "streams.h"
#include "version.h"

#include <librustzcash.h>

using namespace libzcash;

// Construct and populate Sapling note for a given payment address and value.
SaplingNote::SaplingNote(const SaplingPaymentAddress& address, const uint64_t value) :
        BaseNote(value)
{
    d = address.d;
    pk_d = address.pk_d;
    librustzcash_sapling_generate_r(r.begin());
}

// Call librustzcash to compute the commitment
Optional<uint256> SaplingNote::cmu() const
{
    uint256 result;
    if (!librustzcash_sapling_compute_cm(
            d.data(),
            pk_d.begin(),
            value(),
            r.begin(),
            result.begin()
        ))
    {
        return nullopt;
    }

    return result;
}

// Call librustzcash to compute the nullifier
Optional<uint256> SaplingNote::nullifier(const SaplingFullViewingKey& vk, const uint64_t position) const
{
    auto ak = vk.ak;
    auto nk = vk.nk;

    uint256 result;
    if (!librustzcash_sapling_compute_nf(
            d.data(),
            pk_d.begin(),
            value(),
            r.begin(),
            ak.begin(),
            nk.begin(),
            position,
            result.begin()
    ))
    {
        return nullopt;
    }

    return result;
}

// Construct and populate SaplingNotePlaintext for a given note and memo.
SaplingNotePlaintext::SaplingNotePlaintext(
    const SaplingNote& note,
    const std::array<unsigned char, ZC_MEMO_SIZE>& memo) : BaseNotePlaintext(note, memo)
{
    d = note.d;
    rcm = note.r;
}


Optional<SaplingNote> SaplingNotePlaintext::note(const SaplingIncomingViewingKey& ivk) const
{
    auto addr = ivk.address(d);
    if (addr) {
        return SaplingNote(d, addr.get().pk_d, value_, rcm);
    } else {
        return nullopt;
    }
}

Optional<SaplingOutgoingPlaintext> SaplingOutgoingPlaintext::decrypt(
    const SaplingOutCiphertext& ciphertext,
    const uint256& ovk,
    const uint256& cv,
    const uint256& cm,
    const uint256& epk
)
{
    auto pt = AttemptSaplingOutDecryption(ciphertext, ovk, cv, cm, epk);
    if (!pt) {
        return nullopt;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingOutgoingPlaintext ret;
    ss >> ret;

    assert(ss.size() == 0);

    return ret;
}

Optional<SaplingNotePlaintext> SaplingNotePlaintext::decrypt(
    const SaplingEncCiphertext& ciphertext,
    const uint256& ivk,
    const uint256& epk,
    const uint256& cmu
)
{
    auto pt = AttemptSaplingEncDecryption(ciphertext, ivk, epk);
    if (!pt) {
        return nullopt;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingNotePlaintext ret;
    ss >> ret;

    assert(ss.size() == 0);

    uint256 pk_d;
    if (!librustzcash_ivk_to_pkd(ivk.begin(), ret.d.data(), pk_d.begin())) {
        return nullopt;
    }

    uint256 cmu_expected;
    if (!librustzcash_sapling_compute_cm(
        ret.d.data(),
        pk_d.begin(),
        ret.value(),
        ret.rcm.begin(),
        cmu_expected.begin()
    ))
    {
        return nullopt;
    }

    if (cmu_expected != cmu) {
        return nullopt;
    }

    return ret;
}

Optional<SaplingNotePlaintext> SaplingNotePlaintext::decrypt(
    const SaplingEncCiphertext& ciphertext,
    const uint256& epk,
    const uint256& esk,
    const uint256& pk_d,
    const uint256& cmu
)
{
    auto pt = AttemptSaplingEncDecryption(ciphertext, epk, esk, pk_d);
    if (!pt) {
        return nullopt;
    }

    // Deserialize from the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pt.get();

    SaplingNotePlaintext ret;
    ss >> ret;

    uint256 cmu_expected;
    if (!librustzcash_sapling_compute_cm(
        ret.d.data(),
        pk_d.begin(),
        ret.value(),
        ret.rcm.begin(),
        cmu_expected.begin()
    ))
    {
        return nullopt;
    }

    if (cmu_expected != cmu) {
        return nullopt;
    }

    assert(ss.size() == 0);

    return ret;
}

Optional<SaplingNotePlaintextEncryptionResult> SaplingNotePlaintext::encrypt(const uint256& pk_d) const
{
    // Get the encryptor
    auto sne = SaplingNoteEncryption::FromDiversifier(d);
    if (!sne) {
        return nullopt;
    }
    auto enc = sne.get();

    // Create the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << (*this);
    SaplingEncPlaintext pt;
    assert(pt.size() == ss.size());
    memcpy(&pt[0], &ss[0], pt.size());

    // Encrypt the plaintext
    auto encciphertext = enc.encrypt_to_recipient(pk_d, pt);
    if (!encciphertext) {
        return nullopt;
    }
    return SaplingNotePlaintextEncryptionResult(encciphertext.get(), enc);
}


SaplingOutCiphertext SaplingOutgoingPlaintext::encrypt(
        const uint256& ovk,
        const uint256& cv,
        const uint256& cm,
        SaplingNoteEncryption& enc
    ) const
{
    // Create the plaintext
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << (*this);
    SaplingOutPlaintext pt;
    assert(pt.size() == ss.size());
    memcpy(&pt[0], &ss[0], pt.size());

    return enc.encrypt_to_ourselves(ovk, cv, cm, pt);
}
