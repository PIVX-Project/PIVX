// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpivmodule.h"
#include "zpivchain.h"
#include "libzerocoin/Commitment.h"
#include "libzerocoin/Coin.h"
#include "hash.h"
#include "validation.h"
#include "iostream"

bool PublicCoinSpend::Verify(const libzerocoin::Accumulator& a, bool verifyParams) const {
    return validate();
}

bool PublicCoinSpend::validate() const {
    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
    // Check that it opens to the input values
    libzerocoin::Commitment commitment(
            &params->coinCommitmentGroup, getCoinSerialNumber(), randomness);

    if (commitment.getCommitmentValue() != pubCoin.getValue()){
        return error("%s: commitments values are not equal", __func__);
    }
    // Now check that the signature validates with the serial
    if (!HasValidSignature()) {
        return error("%s: signature invalid", __func__);;
    }
    return true;
}

const uint256 PublicCoinSpend::signatureHash() const
{
    CHashWriter h(0, 0);
    h << ptxHash << denomination << getCoinSerialNumber() << randomness << txHash << outputIndex << getSpendType();
    return h.GetHash();
}

namespace ZPIVModule {

    bool createInput(CTxIn &in, CZerocoinMint &mint, uint256 hashTxOut) {
        libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(false);
        uint8_t nVersion = mint.GetVersion();
        if (nVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION) {
            // No v1 serials accepted anymore.
            return error("%s: failed to set zPIV privkey mint version=%d", __func__, nVersion);
        }

        CKey key;
        if (!mint.GetKeyPair(key))
            return error("%s: failed to set zPIV privkey mint version=%d", __func__, nVersion);

        PublicCoinSpend spend(params, mint.GetSerialNumber(), mint.GetRandomness(), key.GetPubKey());
        spend.setTxOutHash(hashTxOut);
        spend.outputIndex = mint.GetOutputIndex();
        spend.txHash = mint.GetTxHash();
        spend.setDenom(mint.GetDenomination());

        std::vector<unsigned char> vchSig;
        if (!key.Sign(spend.signatureHash(), vchSig))
            throw std::runtime_error("ZPIVModule failed to sign signatureHash\n");

        spend.setVchSig(vchSig);

        CDataStream ser(SER_NETWORK, PROTOCOL_VERSION);
        ser << spend;

        std::vector<unsigned char> data(ser.begin(), ser.end());
        CScript scriptSigIn = CScript() << OP_ZEROCOINPUBLICSPEND << data.size();
        scriptSigIn.insert(scriptSigIn.end(), data.begin(), data.end());
        in = CTxIn(mint.GetTxHash(), mint.GetOutputIndex(), scriptSigIn, mint.GetDenomination());
        in.nSequence = mint.GetDenomination();
        return true;
    }

    PublicCoinSpend parseCoinSpend(const CTxIn &in) {
        std::vector<char, zero_after_free_allocator<char> > data;
        data.insert(data.end(), in.scriptSig.begin() + 4, in.scriptSig.end());
        CDataStream serializedCoinSpend(data, SER_NETWORK, PROTOCOL_VERSION);
        libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(false);
        return PublicCoinSpend(params, serializedCoinSpend);
    }

    bool parseCoinSpend(const CTxIn &in, const CTransaction &tx, const CTxOut &prevOut, PublicCoinSpend &publicCoinSpend) {
        if (!in.IsZerocoinPublicSpend() || !prevOut.IsZerocoinMint())
            return error("%s: invalid argument/s", __func__);

        PublicCoinSpend spend = parseCoinSpend(in);
        spend.outputIndex = in.prevout.n;
        spend.txHash = in.prevout.hash;
        CMutableTransaction txNew(tx);
        txNew.vin.clear();
        spend.setTxOutHash(txNew.GetHash());

        // Check prev out now
        CValidationState state;
        if (!TxOutToPublicCoin(prevOut, spend.pubCoin, state))
            return error("%s: cannot get mint from output", __func__);

        spend.setDenom(spend.pubCoin.getDenomination());
        publicCoinSpend = spend;
        return true;
    }

    bool validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction &tx, PublicCoinSpend &publicSpend) {
        // Now prove that the commitment value opens to the input
        if (!parseCoinSpend(in, tx, prevOut, publicSpend)) {
            return false;
        }
        if (libzerocoin::ZerocoinDenominationToAmount(
                libzerocoin::IntToZerocoinDenomination(in.nSequence)) != prevOut.nValue) {
            return error("PublicCoinSpend validateInput :: input nSequence different to prevout value");
        }

        return publicSpend.validate();
    }

    bool ParseZerocoinPublicSpend(const CTxIn &txIn, const CTransaction& tx, CValidationState& state, PublicCoinSpend& publicSpend)
    {
        CTxOut prevOut;
        if(!GetOutput(txIn.prevout.hash, txIn.prevout.n ,state, prevOut)){
            return state.DoS(100, error("%s: public zerocoin spend prev output not found, prevTx %s, index %d",
                                        __func__, txIn.prevout.hash.GetHex(), txIn.prevout.n));
        }
        if (!ZPIVModule::parseCoinSpend(txIn, tx, prevOut, publicSpend)) {
            return state.Invalid(error("%s: invalid public coin spend parse %s\n", __func__,
                                       tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
        }
        return true;
    }
}
