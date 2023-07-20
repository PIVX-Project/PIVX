// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpivmodule.h"

#include "hash.h"
#include "libzerocoin/Commitment.h"
#include "libzerocoin/Coin.h"
#include "validation.h"

template <typename Stream>
PublicCoinSpend::PublicCoinSpend(libzerocoin::ZerocoinParams* params, Stream& strm): pubCoin(params) {
    strm >> *this;
    this->spendType = libzerocoin::SpendType::SPEND;

    if (this->version < PUBSPEND_SCHNORR) {
        // coinVersion is serialized only from v4 spends
        this->coinVersion = libzerocoin::ExtractVersionFromSerial(this->coinSerialNumber);

    } else {
        // from v4 spends, serialNumber is not serialized for v2 coins anymore.
        // in this case, we extract it from the coin public key
        if (this->coinVersion >= libzerocoin::PUBKEY_VERSION)
            this->coinSerialNumber = libzerocoin::ExtractSerialFromPubKey(this->pubkey);

    }

}

bool PublicCoinSpend::Verify() const {
    bool fUseV1Params = getCoinVersion() < libzerocoin::PUBKEY_VERSION;
    if (version < PUBSPEND_SCHNORR) {
        // spend contains the randomness of the coin
        if (fUseV1Params) {
            // Only v2+ coins can publish the randomness
            std::string errMsg = strprintf("PublicCoinSpend version %d with coin version 1 not allowed. "
                    "Minimum spend version required: %d", version, PUBSPEND_SCHNORR);
            return error("%s: %s", __func__, errMsg);
        }

        // Check that the coin is a commitment to serial and randomness.
        libzerocoin::ZerocoinParams* params = Params().GetConsensus().Zerocoin_Params(false);
        libzerocoin::Commitment comm(&params->coinCommitmentGroup, getCoinSerialNumber(), randomness);
        if (comm.getCommitmentValue() != pubCoin.getValue()) {
            return error("%s: commitments values are not equal", __func__);
        }

    } else {
        // for v1 coins, double check that the serialized coin serial is indeed a v1 serial
        if (coinVersion < libzerocoin::PUBKEY_VERSION &&
                libzerocoin::ExtractVersionFromSerial(this->coinSerialNumber) != coinVersion) {
            return error("%s: invalid coin version", __func__);
        }

        // spend contains a shnorr signature of ptxHash with the randomness of the coin
        libzerocoin::ZerocoinParams* params = Params().GetConsensus().Zerocoin_Params(fUseV1Params);
        if (!schnorrSig.Verify(params, getCoinSerialNumber(), pubCoin.getValue(), getTxOutHash())) {
            return error("%s: schnorr signature does not verify", __func__);
        }

    }

    // Now check that the signature validates with the serial
    if (!HasValidSignature()) {
        return error("%s: signature invalid", __func__);;
    }

    return true;
}

bool PublicCoinSpend::HasValidSignature() const
{
    if (coinVersion < libzerocoin::PUBKEY_VERSION)
        return true;

    // for spend version 3 we must check that the provided pubkey and serial number match
    if (version < PUBSPEND_SCHNORR) {
        CBigNum extractedSerial = libzerocoin::ExtractSerialFromPubKey(this->pubkey);
        if (extractedSerial != this->coinSerialNumber)
            return error("%s: hashedpubkey is not equal to the serial!", __func__);
    }

    return pubkey.Verify(signatureHash(), vchSig);
}


const uint256 PublicCoinSpend::signatureHash() const
{
    CHashWriter h(0, 0);
    h << ptxHash << denomination << getCoinSerialNumber() << randomness << txHash << outputIndex << getSpendType();
    return h.GetHash();
}

// 6 comes from OPCODE (1) + vch.size() (1) + BIGNUM size (4)
#define SCRIPT_OFFSET 6

static bool TxOutToPublicCoin(const CTxOut& txout, libzerocoin::PublicCoin& pubCoin, CValidationState& state)
{
    CBigNum publicZerocoin;
    std::vector<unsigned char> vchZeroMint;
    vchZeroMint.insert(vchZeroMint.end(), txout.scriptPubKey.begin() + SCRIPT_OFFSET,
                       txout.scriptPubKey.begin() + txout.scriptPubKey.size());
    publicZerocoin.setvch(vchZeroMint);

    libzerocoin::CoinDenomination denomination = libzerocoin::AmountToZerocoinDenomination(txout.nValue);
    LogPrint(BCLog::LEGACYZC, "%s : denomination %d for pubcoin %s\n", __func__, denomination, publicZerocoin.GetHex());
    if (denomination == libzerocoin::ZQ_ERROR)
        return state.DoS(100, error("%s: txout.nValue is not correct", __func__));

    libzerocoin::PublicCoin checkPubCoin(Params().GetConsensus().Zerocoin_Params(false), publicZerocoin, denomination);
    pubCoin = checkPubCoin;

    return true;
}

// TODO: do not create g_coinspends_cache if the node passed the last zc checkpoint.
class CoinSpendCache {
private:
    mutable Mutex cs;
    std::map<CScript, libzerocoin::CoinSpend> cache_coinspend;
    std::map<CScript, PublicCoinSpend> cache_public_coinspend;

    template<typename T>
    Optional<T> Get(const CScript& in, const std::map<CScript, T>& map) const {
        LOCK(cs);
        auto it = map.find(in);
        return it != map.end() ? Optional<T>{it->second} : nullopt;
    }

public:
    void Add(const CScript& in, libzerocoin::CoinSpend& spend) { WITH_LOCK(cs, cache_coinspend.emplace(in, spend)); }
    void AddPub(const CScript& in, PublicCoinSpend& spend) { WITH_LOCK(cs, cache_public_coinspend.emplace(in, spend)); }

    Optional<libzerocoin::CoinSpend> Get(const CScript& in) const { return Get<libzerocoin::CoinSpend>(in, cache_coinspend); }
    Optional<PublicCoinSpend> GetPub(const CScript& in) const { return Get<PublicCoinSpend>(in, cache_public_coinspend); }
    void Clear() {
        LOCK(cs);
        cache_coinspend.clear();
        cache_public_coinspend.clear();
    }
};
std::unique_ptr<CoinSpendCache> g_coinspends_cache = std::make_unique<CoinSpendCache>();

namespace ZPIVModule {

    // Return stream of CoinSpend from tx input scriptsig
    CDataStream ScriptSigToSerializedSpend(const CScript& scriptSig)
    {
        std::vector<char, zero_after_free_allocator<char> > data;
        // skip opcode and data-len
        uint8_t byteskip = ((uint8_t) scriptSig[1] + 2);
        data.insert(data.end(), scriptSig.begin() + byteskip, scriptSig.end());
        return CDataStream(data, SER_NETWORK, PROTOCOL_VERSION);
    }

    PublicCoinSpend parseCoinSpend(const CTxIn &in)
    {
        libzerocoin::ZerocoinParams *params = Params().GetConsensus().Zerocoin_Params(false);
        CDataStream serializedCoinSpend = ScriptSigToSerializedSpend(in.scriptSig);
        return PublicCoinSpend(params, serializedCoinSpend);
    }

    bool parseCoinSpend(const CTxIn &in, const CTransaction &tx, const CTxOut &prevOut, PublicCoinSpend &publicCoinSpend) {
        if (auto op = g_coinspends_cache->GetPub(in.scriptSig)) {
            publicCoinSpend = *op;
            return true;
        }

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
        g_coinspends_cache->AddPub(in.scriptSig, publicCoinSpend);
        return true;
    }

    libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin)
    {
        if (auto op = g_coinspends_cache->Get(txin.scriptSig)) return *op;
        CDataStream serializedCoinSpend = ScriptSigToSerializedSpend(txin.scriptSig);
        libzerocoin::CoinSpend spend(serializedCoinSpend);
        g_coinspends_cache->Add(txin.scriptSig, spend);
        return spend;
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
        return publicSpend.Verify();
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

    void CleanCoinSpendsCache()
    {
        g_coinspends_cache->Clear();
    }
}
