// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

std::string BaseOutPoint::ToStringShort() const
{
    return strprintf("%s-%u", hash.ToString().substr(0,64), n);
}

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

std::string SaplingOutPoint::ToString() const
{
    return strprintf("SaplingOutPoint(%s, %u)", hash.ToString().substr(0, 10), n);
}


CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

bool CTxIn::IsZerocoinSpend() const
{
    return prevout.hash.IsNull() && scriptSig.IsZerocoinSpend();
}

bool CTxIn::IsZerocoinPublicSpend() const
{
    return scriptSig.IsZerocoinPublicSpend();
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        if(IsZerocoinSpend())
            str += strprintf(", zerocoinspend %s...", HexStr(scriptSig).substr(0, 25));
        else
            str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    nRounds = -10;
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

bool CTxOut::IsZerocoinMint() const
{
    return scriptPubKey.IsZerocoinMint();
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0,30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nType(CTransaction::TxType::NORMAL), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nType(tx.nType), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(*this);
}

size_t CTransaction::DynamicMemoryUsage() const
{
    return memusage::RecursiveDynamicUsage(vin) + memusage::RecursiveDynamicUsage(vout);
}

/* For backward compatibility, the hash is initialized to 0. TODO: remove the need for this default constructor entirely. */
CTransaction::CTransaction() : vin(), vout(), nVersion(CTransaction::CURRENT_VERSION), nType(TxType::NORMAL), nLockTime(0), hash() {}
CTransaction::CTransaction(const CMutableTransaction &tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nType(tx.nType), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion), nType(tx.nType), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload), hash(ComputeHash()) {}

bool CTransaction::HasZerocoinSpendInputs() const
{
    for (const CTxIn& txin: vin) {
        if (txin.IsZerocoinSpend() || txin.IsZerocoinPublicSpend())
            return true;
    }
    return false;
}

bool CTransaction::HasZerocoinMintOutputs() const
{
    for(const CTxOut& txout : vout) {
        if (txout.IsZerocoinMint())
            return true;
    }
    return false;
}

bool CTransaction::IsCoinStake() const
{
    if (vin.empty())
        return false;

    bool fAllowNull = vin[0].IsZerocoinSpend();
    if (vin[0].prevout.IsNull() && !fAllowNull)
        return false;

    return (vout.size() >= 2 && vout[0].IsEmpty());
}

bool CTransaction::HasP2CSOutputs() const
{
    for(const CTxOut& txout : vout) {
        if (txout.scriptPubKey.IsPayToColdStaking())
            return true;
    }
    return false;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const CTxOut& out : vout) {
        // PIVX: previously MoneyRange() was called here. This has been replaced with negative check and boundary wrap check.
        if (out.nValue < 0)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : less than 0");

        if (nValueOut + out.nValue < nValueOut)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : wraps the int64_t boundary");

        nValueOut += out.nValue;
    }

    // Sapling
    if (hasSaplingData() && sapData->valueBalance < 0) {
        // NB: negative valueBalance "takes" money from the transparent value pool just as outputs do
        nValueOut += -sapData->valueBalance;

        // Verify Sapling version
        if (!isSaplingVersion())
            throw std::runtime_error("GetValueOut(): invalid tx version");
    }

    return nValueOut;
}

CAmount CTransaction::GetShieldedValueIn() const
{
    CAmount nValue = 0;

    if (hasSaplingData() && sapData->valueBalance > 0) {
        // NB: positive valueBalance "gives" money to the transparent value pool just as inputs do
        nValue += sapData->valueBalance;

        // Verify Sapling
        if (!isSaplingVersion())
            throw std::runtime_error("GetValueOut(): invalid tx version");
    }

    return nValue;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::ostringstream ss;
    ss << "CTransaction(hash=" << GetHash().ToString().substr(0, 10)
       << ", ver=" << nVersion
       << ", type=" << nType
       << ", vin.size=" << vin.size()
       << ", vout.size=" << vout.size()
       << ", nLockTime=" << nLockTime;
    if (IsShieldedTx()) {
        ss << ", valueBalance=" << sapData->valueBalance
           << ", vShieldedSpend.size=" << sapData->vShieldedSpend.size()
           << ", vShieldedOutput.size=" << sapData->vShieldedOutput.size();
    }
    if (IsSpecialTx()) {
        ss << ", extraPayload.size=" << extraPayload->size();
    }
    ss << ")\n";
    for (const auto& in : vin)
        ss << "    " << in.ToString() << "\n";
    for (const auto& out : vout)
        ss << "    " << out.ToString() << "\n";
    return ss.str();
}

uint256 CalcTxInputsHash(const CTransaction& tx)
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    // transparent inputs
    for (const CTxIn& in: tx.vin) {
        hw << in.prevout;
    }
    // shield inputs
    if (tx.hasSaplingData()) {
        for (const SpendDescription& sd: tx.sapData->vShieldedSpend) {
            hw << sd.nullifier;
        }
    }
    return hw.GetHash();
}
