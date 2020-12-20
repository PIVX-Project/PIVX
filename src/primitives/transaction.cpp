// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "script/standard.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

// contextual flag to guard the serialization for v5 upgrade.
// can be removed once v5 enforcement is activated.
std::atomic<bool> g_IsSaplingActive{false};

std::string BaseOutPoint::ToStringShort() const
{
    return strprintf("%s-%u", hash.ToString().substr(0,64), n);
}

uint256 BaseOutPoint::GetHash() const
{
    return Hash(BEGIN(hash), END(hash), BEGIN(n), END(n));
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

bool CTxOut::GetKeyIDFromUTXO(CKeyID& keyIDRet) const
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (scriptPubKey.empty() || !Solver(scriptPubKey, whichType, vSolutions))
        return false;
    if (whichType == TX_PUBKEY) {
        keyIDRet = CPubKey(vSolutions[0]).GetID();
        return true;
    }
    if (whichType == TX_PUBKEYHASH || whichType == TX_COLDSTAKE) {
        keyIDRet = CKeyID(uint160(vSolutions[0]));
        return true;
    }
    return false;
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
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), nType(tx.nType), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

void CTransaction::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

size_t CTransaction::DynamicMemoryUsage() const
{
    return memusage::RecursiveDynamicUsage(vin) + memusage::RecursiveDynamicUsage(vout);
}

CTransaction::CTransaction() : nVersion(CTransaction::CURRENT_VERSION), nType(TxType::NORMAL), vin(), vout(), nLockTime(0) { }

CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), nType(tx.nType), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload) {
    UpdateHash();
}

CTransaction::CTransaction(CMutableTransaction &&tx) : nVersion(tx.nVersion), nType(tx.nType), vin(std::move(tx.vin)), vout(std::move(tx.vout)), nLockTime(tx.nLockTime), sapData(tx.sapData), extraPayload(tx.extraPayload) {
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int16_t*>(&nVersion) = tx.nVersion;
    *const_cast<int16_t*>(&nType) = tx.nType;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<uint256*>(&hash) = tx.hash;
    *const_cast<Optional<SaplingTxData>*>(&sapData) = tx.sapData;
    return *this;
}

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

bool CTransaction::HasZerocoinPublicSpendInputs() const
{
    // The wallet only allows publicSpend inputs in the same tx and not a combination between piv and zpiv
    for(const CTxIn& txin : vin) {
        if (txin.IsZerocoinPublicSpend())
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

bool CTransaction::CheckColdStake(const CScript& script) const
{

    // tx is a coinstake tx
    if (!IsCoinStake())
        return false;

    // all inputs have the same scriptSig
    CScript firstScript = vin[0].scriptSig;
    if (vin.size() > 1) {
        for (unsigned int i=1; i<vin.size(); i++)
            if (vin[i].scriptSig != firstScript) return false;
    }

    // all outputs except first (coinstake marker) and last (masternode payout)
    // have the same pubKeyScript and it matches the script we are spending
    if (vout[1].scriptPubKey != script) return false;
    if (vin.size() > 3) {
        for (unsigned int i=2; i<vout.size()-1; i++)
            if (vout[i].scriptPubKey != script) return false;
    }

    return true;
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
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it) {
        // PIVX: previously MoneyRange() was called here. This has been replaced with negative check and boundary wrap check.
        if (it->nValue < 0)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : less than 0");

        if ((nValueOut + it->nValue) < nValueOut)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : wraps the int64_t boundary");

        nValueOut += it->nValue;
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

CAmount CTransaction::GetShieldValueIn() const
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

CAmount CTransaction::GetZerocoinSpent() const
{
    CAmount nValueOut = 0;
    for (const CTxIn& txin : vin) {
        if(!txin.IsZerocoinSpend())
            continue;

        nValueOut += txin.nSequence * COIN;
    }

    return nValueOut;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        unsigned int offset = 41U + std::min(110U, (unsigned int)it->scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    return nTxSize;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
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
    if (IsShieldTx()) {
        ss << ", valueBalance=" << sapData->valueBalance
           << ", vShieldSpend.size=" << sapData->vShieldSpend.size()
           << ", vShieldOutput.size=" << sapData->vShieldOutput.size();
    }
    if (IsSpecialTx()) {
        ss << ", extraPayload.size=" << extraPayload->size();
    }
    ss << ")\n";
    for (unsigned int i = 0; i < vin.size(); i++)
        ss << "    " << vin[i].ToString() << "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        ss << "    " << vout[i].ToString() << "\n";
    return ss.str();
}
