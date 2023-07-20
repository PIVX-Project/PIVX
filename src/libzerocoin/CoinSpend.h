/**
 * @file       CoinSpend.h
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

#ifndef COINSPEND_H_
#define COINSPEND_H_

#include <streams.h>
#include <utilstrencodings.h>
#include "Coin.h"
#include "Commitment.h"
#include "Params.h"
#include "SpendType.h"

#include "bignum.h"
#include "pubkey.h"
#include "serialize.h"

namespace libzerocoin
{
// Lagacy zPIV - Only for serialization
// Proof that a value inside a commitment C is accumulated in accumulator A
class AccumulatorProofOfKnowledge {
public:
    AccumulatorProofOfKnowledge() {};
    SERIALIZE_METHODS(AccumulatorProofOfKnowledge, obj) {
        READWRITE(obj.C_e, obj.C_u, obj.C_r, obj.st_1, obj.st_2, obj.st_3);
        READWRITE(obj.t_1, obj.t_2, obj.t_3, obj.t_4, obj.s_alpha, obj.s_beta);
        READWRITE(obj.s_zeta, obj.s_sigma, obj.s_eta, obj.s_epsilon);
        READWRITE(obj.s_delta, obj.s_xi, obj.s_phi, obj.s_gamma, obj.s_psi);
    }
private:
    CBigNum C_e, C_u, C_r;
    CBigNum st_1, st_2, st_3;
    CBigNum t_1, t_2, t_3, t_4;
    CBigNum s_alpha, s_beta, s_zeta, s_sigma, s_eta, s_epsilon, s_delta;
    CBigNum s_xi, s_phi, s_gamma, s_psi;
};

// Lagacy zPIV - Only for serialization
// Signature of knowledge attesting that the signer knows the values to
// open a commitment to a coin with given serial number
class SerialNumberSignatureOfKnowledge {
public:
    SerialNumberSignatureOfKnowledge(){};
    SERIALIZE_METHODS(SerialNumberSignatureOfKnowledge, obj) { READWRITE(obj.s_notprime, obj.sprime, obj.hash); }
private:
    uint256 hash;
    std::vector<CBigNum> s_notprime;
    std::vector<CBigNum> sprime;
};

// Lagacy zPIV - Only for serialization
// Proof that two commitments open to the same value (BROKEN)
class CommitmentProofOfKnowledge {
public:
    CommitmentProofOfKnowledge() {};
    SERIALIZE_METHODS(CommitmentProofOfKnowledge, obj) { READWRITE(obj.S1,obj.S2, obj.S3, obj.challenge); }
private:
    CBigNum S1, S2, S3, challenge;
};


// Lagacy zPIV - Only for serialization
/** The complete proof needed to spend a zerocoin.
 * Composes together a proof that a coin is accumulated
 * and that it has a given serial number.
 */
class CoinSpend
{
public:

    CoinSpend() {};
    CoinSpend(CDataStream& strm) { strm >> *this; }
    virtual ~CoinSpend(){};

    const CBigNum& getCoinSerialNumber() const { return this->coinSerialNumber; }
    CoinDenomination getDenomination() const { return this->denomination; }
    uint32_t getAccumulatorChecksum() const { return this->accChecksum; }
    uint256 getTxOutHash() const { return ptxHash; }
    CBigNum getAccCommitment() const { return accCommitmentToCoinValue; }
    CBigNum getSerialComm() const { return serialCommitmentToCoinValue; }
    uint8_t getVersion() const { return version; }
    int getCoinVersion() const { return libzerocoin::ExtractVersionFromSerial(coinSerialNumber); }
    CPubKey getPubKey() const { return pubkey; }
    SpendType getSpendType() const { return spendType; }
    std::vector<unsigned char> getSignature() const { return vchSig; }

    static std::vector<unsigned char> ParseSerial(CDataStream& s);

    virtual const uint256 signatureHash() const;
    bool HasValidSerial(ZerocoinParams* params) const;
    bool HasValidSignature() const;
    void setTxOutHash(uint256 txOutHash) { this->ptxHash = txOutHash; };
    void setDenom(libzerocoin::CoinDenomination denom) { this->denomination = denom; }
    void setPubKey(CPubKey pkey, bool fUpdateSerial = false);

    CBigNum CalculateValidSerial(ZerocoinParams* params);
    std::string ToString() const;

    SERIALIZE_METHODS(CoinSpend, obj)
    {
        READWRITE(obj.denomination, obj.ptxHash, obj.accChecksum, obj.accCommitmentToCoinValue);
        READWRITE(obj.serialCommitmentToCoinValue, obj.coinSerialNumber, obj.accumulatorPoK);
        READWRITE(obj.serialNumberSoK, obj.commitmentPoK);
        try {
            READWRITE(obj.version, obj.pubkey, obj.vchSig, obj.spendType);
        } catch (...) {
            SER_READ(obj, obj.version = 1);
        }
    }

protected:
    CoinDenomination denomination{ZQ_ERROR};
    CBigNum coinSerialNumber{};
    uint8_t version{0};
    //As of version 2
    CPubKey pubkey;
    std::vector<unsigned char> vchSig;
    SpendType spendType{SPEND};
    uint256 ptxHash{UINT256_ZERO};

private:
    uint32_t accChecksum{0};
    CBigNum accCommitmentToCoinValue;
    CBigNum serialCommitmentToCoinValue;
    AccumulatorProofOfKnowledge accumulatorPoK;
    SerialNumberSignatureOfKnowledge serialNumberSoK;
    CommitmentProofOfKnowledge commitmentPoK;

};

} /* namespace libzerocoin */
#endif /* COINSPEND_H_ */
