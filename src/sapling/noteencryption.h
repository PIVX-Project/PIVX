// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

/*
See the Zcash protocol specification for more information.
https://github.com/zcash/zips/blob/master/protocol/protocol.pdf
*/

#ifndef ZC_NOTE_ENCRYPTION_H_
#define ZC_NOTE_ENCRYPTION_H_

#include "optional.h"
#include "sapling/sapling.h"
#include "uint256.h"

#include <array>

namespace libzcash {

// Ciphertext for the recipient to decrypt
typedef std::array<unsigned char, ZC_SAPLING_ENCCIPHERTEXT_SIZE> SaplingEncCiphertext;
typedef std::array<unsigned char, ZC_SAPLING_ENCPLAINTEXT_SIZE> SaplingEncPlaintext;

// Ciphertext for outgoing viewing key to decrypt
typedef std::array<unsigned char, ZC_SAPLING_OUTCIPHERTEXT_SIZE> SaplingOutCiphertext;
typedef std::array<unsigned char, ZC_SAPLING_OUTPLAINTEXT_SIZE> SaplingOutPlaintext;

//! This is not a thread-safe API.
class SaplingNoteEncryption {
protected:
    // Ephemeral public key
    uint256 epk;

    // Ephemeral secret key
    uint256 esk;

    bool already_encrypted_enc;
    bool already_encrypted_out;

    SaplingNoteEncryption(uint256 epk, uint256 esk) : epk(epk), esk(esk), already_encrypted_enc(false), already_encrypted_out(false) {

    }

public:

    static Optional<SaplingNoteEncryption> FromDiversifier(diversifier_t d);

    Optional<SaplingEncCiphertext> encrypt_to_recipient(
        const uint256 &pk_d,
        const SaplingEncPlaintext &message
    );

    SaplingOutCiphertext encrypt_to_ourselves(
        const uint256 &ovk,
        const uint256 &cv,
        const uint256 &cm,
        const SaplingOutPlaintext &message
    );

    uint256 get_epk() const {
        return epk;
    }

    uint256 get_esk() const {
        return esk;
    }
};

// Attempts to decrypt a Sapling note. This will not check that the contents
// of the ciphertext are correct.
Optional<SaplingEncPlaintext> AttemptSaplingEncDecryption(
    const SaplingEncCiphertext &ciphertext,
    const uint256 &ivk,
    const uint256 &epk
);

// Attempts to decrypt a Sapling note using outgoing plaintext.
// This will not check that the contents of the ciphertext are correct.
Optional<SaplingEncPlaintext> AttemptSaplingEncDecryption (
    const SaplingEncCiphertext &ciphertext,
    const uint256 &epk,
    const uint256 &esk,
    const uint256 &pk_d
);

// Attempts to decrypt a Sapling note. This will not check that the contents
// of the ciphertext are correct.
Optional<SaplingOutPlaintext> AttemptSaplingOutDecryption(
    const SaplingOutCiphertext &ciphertext,
    const uint256 &ovk,
    const uint256 &cv,
    const uint256 &cm,
    const uint256 &epk
);

}

#endif /* ZC_NOTE_ENCRYPTION_H_ */
