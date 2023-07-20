/**
 * @file       Coin.h
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

#ifndef COIN_H_
#define COIN_H_
#include "Denominations.h"
#include "Params.h"
#include "amount.h"
#include "bignum.h"
#include "util/system.h"
#include "key.h"

namespace libzerocoin
{
    static int constexpr PUBKEY_VERSION = 2;
    static int constexpr CURRENT_VERSION = 2;
    static int constexpr V2_BITSHIFT = 4;

    class InvalidSerialException : public std::exception {
    public:
        std::string message;
        InvalidSerialException(const std::string &message) : message(message) {}
    };

    int ExtractVersionFromSerial(const CBigNum& bnSerial);
    bool IsValidSerial(const ZerocoinParams* params, const CBigNum& bnSerial);
    bool IsValidCommitmentToCoinRange(const ZerocoinParams* params, const CBigNum& bnCommitment);
    CBigNum GetAdjustedSerial(const CBigNum& bnSerial);
    CBigNum ExtractSerialFromPubKey(const CPubKey pubkey);

/** A Public coin is the part of a coin that
 * is published to the network and what is handled
 * by other clients. It contains only the value
 * of commitment to a serial number and the
 * denomination of the coin.
 */
class PublicCoin
{
public:
    template <typename Stream>
    PublicCoin(const ZerocoinParams* p, Stream& strm) : params(p)
    {
        strm >> *this;
    }

    PublicCoin(const ZerocoinParams* p);

    /**Generates a public coin
     *
     * @param p cryptographic paramters
     * @param coin the value of the commitment.
     * @param denomination The denomination of the coin.
     */
    PublicCoin(const ZerocoinParams* p, const CBigNum& coin, const CoinDenomination d);
    const CBigNum& getValue() const { return this->value; }

    CoinDenomination getDenomination() const { return this->denomination; }
    bool operator==(const PublicCoin& rhs) const
    {
        return ((this->value == rhs.value) && (this->params == rhs.params) && (this->denomination == rhs.denomination));
    }
    bool operator!=(const PublicCoin& rhs) const { return !(*this == rhs); }
    /** Checks that coin is prime and in the appropriate range given the parameters
     * @return true if valid
     */
    bool validate() const;

    SERIALIZE_METHODS(PublicCoin, obj) { READWRITE(obj.value, obj.denomination); }

private:
    const ZerocoinParams* params;
    CBigNum value;
    CoinDenomination denomination;
};

} /* namespace libzerocoin */
#endif /* COIN_H_ */
