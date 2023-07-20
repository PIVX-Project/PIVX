/**
 * @file       Commitment.h
 *
 * @brief      Commitment and CommitmentProof classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2021 The hemis Core developers

#ifndef COMMITMENT_H_
#define COMMITMENT_H_

#include "Params.h"
#include "serialize.h"

// We use a SHA256 hash for our PoK challenges. Update the following
// if we ever change hash functions.
#define COMMITMENT_EQUALITY_CHALLENGE_SIZE  256

// A 512-bit security parameter for the statistical ZK PoK.
#define COMMITMENT_EQUALITY_SECMARGIN       512

namespace libzerocoin {

/**
 * A commitment, complete with contents and opening randomness.
 * These should remain secret. Publish only the commitment value.
 */
class Commitment {
public:
    explicit Commitment(const IntegerGroupParams* p, const CBigNum& bnSerial, const CBigNum& bnRandomness):
                params(p),
                randomness(bnRandomness),
                contents(bnSerial)
    {
        this->commitmentValue = (params->g.pow_mod(this->contents, params->modulus).mul_mod(
                                 params->h.pow_mod(this->randomness, params->modulus), params->modulus));
    }

    Commitment(const IntegerGroupParams* p, const CBigNum& value):
        Commitment(p, value, CBigNum::randBignum(p->groupOrder)) {};

    const CBigNum& getCommitmentValue() const { return this->commitmentValue; };
    const CBigNum& getRandomness() const { return this->randomness; };
    const CBigNum& getContents() const { return this->contents; };

private:
    const IntegerGroupParams *params;
    CBigNum commitmentValue;
    CBigNum randomness;
    const CBigNum contents;

    SERIALIZE_METHODS(Commitment, obj) { READWRITE(obj.commitmentValue, obj.randomness, obj.contents); }
};
} /* namespace libzerocoin */
#endif /* COMMITMENT_H_ */
