/**
 * @file       CoinSpend.cpp
 *
 * @brief      CoinSpend class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2021 The hemis Core developers

#include "CoinSpend.h"
#include <iostream>
#include <sstream>

namespace libzerocoin
{

const uint256 CoinSpend::signatureHash() const
{
    CHashWriter h(0, 0);
    h << serialCommitmentToCoinValue << accCommitmentToCoinValue << commitmentPoK << accumulatorPoK << ptxHash
      << coinSerialNumber << accChecksum << denomination;

    if (version >= PUBKEY_VERSION)
        h << spendType;

    return h.GetHash();
}

std::string CoinSpend::ToString() const
{
    std::stringstream ss;
    ss << "CoinSpend:\n version=" << (int)version << " signatureHash=" << signatureHash().GetHex() << " spendtype=" << spendType << "\n";
    return ss.str();
}

bool CoinSpend::HasValidSerial(ZerocoinParams* params) const
{
    return IsValidSerial(params, coinSerialNumber);
}

//Additional verification layer that requires the spend be signed by the private key associated with the serial
bool CoinSpend::HasValidSignature() const
{
    const int coinVersion = getCoinVersion();
    //No private key for V1
    if (coinVersion < PUBKEY_VERSION)
        return true;

    try {
        //V2 serial requires that the signature hash be signed by the public key associated with the serial
        arith_uint256 hashedPubkey = UintToArith256(Hash(pubkey.begin(), pubkey.end())) >> V2_BITSHIFT;
        if (hashedPubkey != GetAdjustedSerial(coinSerialNumber).getuint256()) {
            //cout << "CoinSpend::HasValidSignature() hashedpubkey is not equal to the serial!\n";
            return false;
        }
    } catch (const std::range_error& e) {
        //std::cout << "HasValidSignature() error: " << e.what() << std::endl;
        throw InvalidSerialException("Serial longer than 256 bits");
    }

    return pubkey.Verify(signatureHash(), vchSig);
}

CBigNum CoinSpend::CalculateValidSerial(ZerocoinParams* params)
{
    CBigNum bnSerial = coinSerialNumber;
    bnSerial = bnSerial % params->coinCommitmentGroup.groupOrder;
    return bnSerial;
}

std::vector<unsigned char> CoinSpend::ParseSerial(CDataStream& s) {
    unsigned int nSize = ReadCompactSize(s);
    s.movePos(nSize);
    nSize = ReadCompactSize(s);
    s.movePos(nSize);
    CBigNum coinSerialNumber;
    s >> coinSerialNumber;
    return coinSerialNumber.getvch();
}

void CoinSpend::setPubKey(CPubKey pkey, bool fUpdateSerial) {
    this->pubkey = pkey;
    if (fUpdateSerial) {
        this->coinSerialNumber = libzerocoin::ExtractSerialFromPubKey(this->pubkey);
    }
}

} /* namespace libzerocoin */
