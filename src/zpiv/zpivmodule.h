// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#ifndef hemis_ZHMSMODULE_H
#define hemis_ZHMSMODULE_H

#include "libzerocoin/bignum.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/CoinRandomnessSchnorrSignature.h"
#include "libzerocoin/SpendType.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"
#include <streams.h>
#include <utilstrencodings.h>
#include "chainparams.h"

static int const PUBSPEND_SCHNORR = 4;

class PublicCoinSpend : public libzerocoin::CoinSpend {
public:

    PublicCoinSpend(libzerocoin::ZerocoinParams* params): pubCoin(params) {};
    template <typename Stream> PublicCoinSpend(libzerocoin::ZerocoinParams* params, Stream& strm);

    ~PublicCoinSpend(){};

    const uint256 signatureHash() const override;
    bool HasValidSignature() const;
    bool Verify() const;
    int getCoinVersion() const { return this->coinVersion; }

    // Members
    int coinVersion;
    CBigNum randomness;
    libzerocoin::CoinRandomnessSchnorrSignature schnorrSig;
    // prev out values
    uint256 txHash;
    unsigned int outputIndex = -1;
    libzerocoin::PublicCoin pubCoin;

    SERIALIZE_METHODS(PublicCoinSpend, obj) {
        READWRITE(obj.version);
        if (obj.version < PUBSPEND_SCHNORR) {
            READWRITE(obj.coinSerialNumber, obj.randomness, obj.pubkey, obj.vchSig);
        } else {
            READWRITE(obj.coinVersion);
            if (obj.coinVersion < libzerocoin::PUBKEY_VERSION) {
                READWRITE(obj.coinSerialNumber);
            } else {
                READWRITE(obj.pubkey, obj.vchSig);
            }
            READWRITE(obj.schnorrSig);
        }
    }
};


class CValidationState;

namespace ZHMSModule {
    CDataStream ScriptSigToSerializedSpend(const CScript& scriptSig);
    PublicCoinSpend parseCoinSpend(const CTxIn &in);
    bool parseCoinSpend(const CTxIn &in, const CTransaction& tx, const CTxOut &prevOut, PublicCoinSpend& publicCoinSpend);
    libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin);
    bool validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction& tx, PublicCoinSpend& ret);

    // Public zc spend parse
    /**
     *
     * @param in --> public zc spend input
     * @param tx --> input parent
     * @param publicCoinSpend ---> return the publicCoinSpend parsed
     * @return true if everything went ok
     */
    bool ParseZerocoinPublicSpend(const CTxIn &in, const CTransaction& tx, CValidationState& state, PublicCoinSpend& publicCoinSpend);

    // Clear the coinspend cache
    void CleanCoinSpendsCache();
};


#endif //hemis_ZHMSMODULE_H
