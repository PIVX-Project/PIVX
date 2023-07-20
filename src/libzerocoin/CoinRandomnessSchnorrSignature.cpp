// Copyright (c) 2019 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CoinRandomnessSchnorrSignature.h"

namespace libzerocoin {

CoinRandomnessSchnorrSignature::CoinRandomnessSchnorrSignature(
        const ZerocoinParams* zcparams, const CBigNum randomness, const uint256 msghash)
{
    const CBigNum p = zcparams->coinCommitmentGroup.modulus;
    const CBigNum q = zcparams->coinCommitmentGroup.groupOrder;
    const CBigNum h = zcparams->coinCommitmentGroup.h;
    const CBigNum pk = h.pow_mod(randomness, p);

    alpha = 0;
    beta = 0;

    CBigNum k, r;

    while (!alpha || !beta) {
        // select random nonce k in Zq and let r = h^k mod p
        k = CBigNum::randBignum(q);
        r = h.pow_mod(k, p);

        // challenge hash
        CHashWriter hasher(0,0);
        hasher << *zcparams << pk << r << msghash;
        alpha = CBigNum(hasher.GetHash()) % q;
        beta = (k - alpha.mul_mod(randomness, q)) % q;
    }

}

bool CoinRandomnessSchnorrSignature::Verify(
        const ZerocoinParams* zcparams, const CBigNum& S, const CBigNum& C, const uint256 msghash) const
{
    const CBigNum p = zcparams->coinCommitmentGroup.modulus;
    const CBigNum q = zcparams->coinCommitmentGroup.groupOrder;
    const CBigNum h = zcparams->coinCommitmentGroup.h;
    const CBigNum g = zcparams->coinCommitmentGroup.g;

    // Params validation.
    if (!IsValidSerial(zcparams, S)) return error("%s: Invalid serial range", __func__);
    if (alpha < BN_ZERO || alpha >= q) return error("%s: alpha out of range", __func__);
    if (beta < BN_ZERO || beta >= q) return error("%s: beta out of range", __func__);

    // Schnorr public key computation.
    const CBigNum pk = C.mul_mod(g.pow_mod(-S,p),p);

    // Signature verification.
    const CBigNum rv = (pk.pow_mod(alpha,p)).mul_mod(h.pow_mod(beta,p),p);
    CHashWriter hasher(0,0);
    hasher << *zcparams << pk << rv << msghash;

    if (CBigNum(hasher.GetHash()) % q != alpha)
        return error("%s: Schnorr signature does not verify", __func__);

    return true;

}

} /* namespace libzerocoin */
