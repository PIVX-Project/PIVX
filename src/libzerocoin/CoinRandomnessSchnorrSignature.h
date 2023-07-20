// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COINRANDOMNESSPROOF_H_
#define COINRANDOMNESSPROOF_H_

#include "Params.h"
#include "Coin.h"
#include "serialize.h"
#include "hash.h"

namespace libzerocoin {

/**A Schnorr Signature on the hash of metadata attesting that the signer knows the randomness v
 *  necessary to open a public coin C (which is a pedersen commitment g^S h^v mod p) with
 * given serial number S.
 */
class CoinRandomnessSchnorrSignature {
public:
    CoinRandomnessSchnorrSignature() {};
    template <typename Stream> CoinRandomnessSchnorrSignature(Stream& strm) {strm >> *this;}

    /** Creates a Schnorr signature object using the randomness of a public coin as private key sk.
     *
     * @param zcparams zerocoin params (group modulus, order and generators)
     * @param randomness the coin we are going to use for the signature (sk := randomness, pk := h^sk mod p).
     * @param msghash hash of meta data to create a signature on.
     */
    CoinRandomnessSchnorrSignature(const ZerocoinParams* zcparams, const CBigNum randomness, const uint256 msghash);

    /** Verifies the Schnorr signature on message msghash with public key pk = Cg^-S mod p
     *
     * @param zcparams zerocoin params (group modulus, order and generators)
     * @param S serial number of the coin used for the signature
     * @param C value of the public coin (commitment to serial S and randomness v) used for the signature.
     * @param msghash hash of meta data to verify the signature on.
     * @return
     */
    bool Verify(const ZerocoinParams* zcparams, const CBigNum& S, const CBigNum& C, const uint256 msghash) const;

    SERIALIZE_METHODS(CoinRandomnessSchnorrSignature, obj) { READWRITE(obj.alpha, obj.beta); }

private:
    // signature components
    CBigNum alpha, beta;
};

} /* namespace libzerocoin */
#endif /* COINRANDOMNESSPROOF_H_ */
