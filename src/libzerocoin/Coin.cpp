/**
 * @file       Coin.cpp
 *
 * @brief      PublicCoin class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2021 The hemis Core developers

#include <stdexcept>
#include <iostream>
#include "Coin.h"
#include "Commitment.h"
#include "pubkey.h"

namespace libzerocoin {

//PublicCoin class
PublicCoin::PublicCoin(const ZerocoinParams* p):
    params(p) {
    if (!this->params->initialized) {
        throw std::runtime_error("Params are not initialized");
    }
    // Assume this will get set by another method later
    denomination = ZQ_ERROR;
};

PublicCoin::PublicCoin(const ZerocoinParams* p, const CBigNum& coin, const CoinDenomination d):
    params(p), value(coin), denomination(d) {
    if (!this->params->initialized) {
        throw std::runtime_error("Params are not initialized");
    }
    if (denomination == 0) {
        throw std::runtime_error("Denomination does not exist");
    }
};

bool PublicCoin::validate() const
{
    if (this->params->accumulatorParams.minCoinValue >= value) {
        return error("%s: ERROR: PublicCoin::validate value is too low: %s", __func__, value.GetDec());
    }

    if (value > this->params->accumulatorParams.maxCoinValue) {
        return error("%s: ERROR: PublicCoin::validate value is too high, max: %s, received: %s",
                __func__, this->params->accumulatorParams.maxCoinValue, value.GetDec());
    }

    if (!value.isPrime(params->zkp_iterations)) {
        return error("%s: ERROR: PublicCoin::validate value is not prime. Value: %s, Iterations: %d",
                __func__, value.GetDec(), params->zkp_iterations);
    }

    return true;
}

int ExtractVersionFromSerial(const CBigNum& bnSerial)
{
    try {
        //Serial is marked as v2 only if the first byte is 0xF
        arith_uint256 nMark = bnSerial.getuint256() >> (256 - V2_BITSHIFT);
        if (nMark == arith_uint256(0xf))
            return PUBKEY_VERSION;
    } catch (const std::range_error& e) {
        //std::cout << "ExtractVersionFromSerial(): " << e.what() << std::endl;
        // Only serial version 2 appeared with this range error..
        return 2;
    }

    return 1;
}

//Remove the first four bits for V2 serials
CBigNum GetAdjustedSerial(const CBigNum& bnSerial)
{
    const uint256& serial = ArithToUint256(bnSerial.getuint256() & (~ARITH_UINT256_ZERO >> V2_BITSHIFT));
    CBigNum bnSerialAdjusted;
    bnSerialAdjusted.setuint256(serial);
    return bnSerialAdjusted;
}


bool IsValidSerial(const ZerocoinParams* params, const CBigNum& bnSerial)
{
    if (bnSerial <= 0)
        return false;

    if (ExtractVersionFromSerial(bnSerial) < PUBKEY_VERSION)
        return bnSerial < params->coinCommitmentGroup.groupOrder;

    // If V2, the serial is marked with 0xF in the first 4 bits. So It's always > groupOrder.
    // This is removed for the adjusted serial - so it's always < groupOrder.
    // So we check only the bitsize here.
    return bnSerial.bitSize() <= 256;
}


bool IsValidCommitmentToCoinRange(const ZerocoinParams* params, const CBigNum& bnCommitment)
{
    return bnCommitment > BN_ZERO && bnCommitment < params->serialNumberSoKCommitmentGroup.modulus;
}


CBigNum ExtractSerialFromPubKey(const CPubKey pubkey)
{
    const arith_uint256& hashedPubkey = UintToArith256(Hash(pubkey.begin(), pubkey.end())) >> V2_BITSHIFT;
    arith_uint256 uintSerial = (arith_uint256(0xF) << (256 - V2_BITSHIFT)) | hashedPubkey;
    return CBigNum(ArithToUint256(uintSerial));
}


} /* namespace libzerocoin */
