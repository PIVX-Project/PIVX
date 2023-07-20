// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/providertx.h"

#include "bls/key_io.h"
#include "key_io.h"

std::string ProRegPL::MakeSignString() const
{
    std::ostringstream ss;

    ss << HexStr(scriptPayout) << "|";
    ss << strprintf("%d", nOperatorReward) << "|";
    ss << EncodeDestination(keyIDOwner) << "|";
    ss << EncodeDestination(keyIDVoting) << "|";

    // ... and also the full hash of the payload as a protection agains malleability and replays
    ss << ::SerializeHash(*this).ToString();

    return ss.str();
}

std::string ProRegPL::ToString() const
{
    CTxDestination dest;
    std::string payee = ExtractDestination(scriptPayout, dest) ?
                        EncodeDestination(dest) : "unknown";
    return strprintf("ProRegPL(nVersion=%d, collateralOutpoint=%s, addr=%s, nOperatorReward=%f, ownerAddress=%s, operatorPubKey=%s, votingAddress=%s, scriptPayout=%s)",
        nVersion, collateralOutpoint.ToStringShort(), addr.ToString(), (double)nOperatorReward / 100, EncodeDestination(keyIDOwner), bls::EncodePublic(Params(), pubKeyOperator), EncodeDestination(keyIDVoting), payee);
}

void ProRegPL::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("version", nVersion);
    obj.pushKV("collateralHash", collateralOutpoint.hash.ToString());
    obj.pushKV("collateralIndex", (int)collateralOutpoint.n);
    obj.pushKV("service", addr.ToString());
    obj.pushKV("ownerAddress", EncodeDestination(keyIDOwner));
    obj.pushKV("operatorPubKey", bls::EncodePublic(Params(), pubKeyOperator));
    obj.pushKV("votingAddress", EncodeDestination(keyIDVoting));

    CTxDestination dest1;
    if (ExtractDestination(scriptPayout, dest1)) {
        obj.pushKV("payoutAddress", EncodeDestination(dest1));
    }
    CTxDestination dest2;
    if (ExtractDestination(scriptOperatorPayout, dest2)) {
        obj.pushKV("operatorPayoutAddress", EncodeDestination(dest2));
    }
    obj.pushKV("operatorReward", (double)nOperatorReward / 100);
    obj.pushKV("inputsHash", inputsHash.ToString());
}

std::string ProUpServPL::ToString() const
{
    CTxDestination dest;
    std::string payee = ExtractDestination(scriptOperatorPayout, dest) ?
                        EncodeDestination(dest) : "unknown";
    return strprintf("ProUpServPL(nVersion=%d, proTxHash=%s, addr=%s, operatorPayoutAddress=%s)",
        nVersion, proTxHash.ToString(), addr.ToString(), payee);
}

void ProUpServPL::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("version", nVersion);
    obj.pushKV("proTxHash", proTxHash.ToString());
    obj.pushKV("service", addr.ToString());
    CTxDestination dest;
    if (ExtractDestination(scriptOperatorPayout, dest)) {
        obj.pushKV("operatorPayoutAddress", EncodeDestination(dest));
    }
    obj.pushKV("inputsHash", inputsHash.ToString());
}

std::string ProUpRegPL::ToString() const
{
    CTxDestination dest;
    std::string payee = ExtractDestination(scriptPayout, dest) ?
                        EncodeDestination(dest) : "unknown";
    return strprintf("ProUpRegPL(nVersion=%d, proTxHash=%s, operatorPubKey=%s, votingAddress=%s, payoutAddress=%s)",
        nVersion, proTxHash.ToString(), bls::EncodePublic(Params(), pubKeyOperator), EncodeDestination(keyIDVoting), payee);
}

void ProUpRegPL::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("version", nVersion);
    obj.pushKV("proTxHash", proTxHash.ToString());
    obj.pushKV("votingAddress", EncodeDestination(keyIDVoting));
    CTxDestination dest;
    if (ExtractDestination(scriptPayout, dest)) {
        obj.pushKV("payoutAddress", EncodeDestination(dest));
    }
    obj.pushKV("operatorPubKey", bls::EncodePublic(Params(), pubKeyOperator));
    obj.pushKV("inputsHash", inputsHash.ToString());
}

std::string ProUpRevPL::ToString() const
{
    return strprintf("ProUpRevPL(nVersion=%d, proTxHash=%s, nReason=%d)",
                      nVersion, proTxHash.ToString(), nReason);
}

void ProUpRevPL::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("version", nVersion);
    obj.pushKV("proTxHash", proTxHash.ToString());
    obj.pushKV("reason", (int)nReason);
    obj.pushKV("inputsHash", inputsHash.ToString());
}

bool GetProRegCollateral(const CTransactionRef& tx, COutPoint& outRet)
{
    if (tx == nullptr) {
        return false;
    }
    if (!tx->IsSpecialTx() || tx->nType != CTransaction::TxType::PROREG) {
        return false;
    }
    ProRegPL pl;
    if (!GetTxPayload(*tx, pl)) {
        return false;
    }
    outRet = pl.collateralOutpoint.hash.IsNull() ? COutPoint(tx->GetHash(), pl.collateralOutpoint.n)
                                                 : pl.collateralOutpoint;
    return true;
}


