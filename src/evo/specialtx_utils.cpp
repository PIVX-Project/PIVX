// Copyright (c) 2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or htts://www.opensource.org/licenses/mit-license.php.

#include "evo/specialtx_utils.h"

#include "net.h"
#include "script/script.h"
#include "wallet/wallet.h"

#ifdef ENABLE_WALLET

OperationResult FundSpecialTx(CWallet* pwallet, CMutableTransaction& tx)
{
    static CTxOut dummyTxOut(0, CScript() << OP_RETURN);
    std::vector<CRecipient> vecSend;
    bool dummyTxOutAdded = false;

    if (tx.vout.empty()) {
        // add dummy txout as CreateTransaction requires at least one recipient
        tx.vout.emplace_back(dummyTxOut);
        dummyTxOutAdded = true;
    }

    CAmount nFee;
    CFeeRate feeRate = CFeeRate(0);
    int nChangePos = -1;
    std::string strFailReason;
    std::set<int> setSubtractFeeFromOutputs;
    if (!pwallet->FundTransaction(tx, nFee, false, feeRate, nChangePos, strFailReason, false, false, {})) {
        return {false, strFailReason};
    }

    if (dummyTxOutAdded && tx.vout.size() > 1) {
        // FundTransaction added a change output, so we don't need the dummy txout anymore
        // Removing it results in slight overpayment of fees, but we ignore this for now (as it's a very low amount)
        auto it = std::find(tx.vout.begin(), tx.vout.end(), dummyTxOut);
        assert(it != tx.vout.end());
        tx.vout.erase(it);
    }

    return {true};
}

OperationResult SignAndSendSpecialTx(CWallet* pwallet, CMutableTransaction& tx, std::map<std::string, std::string>* extras)
{
    if (!pwallet->SignTransaction(tx)) {
        return {false, "signature failed"};
    }

    CWallet::CommitResult res = pwallet->CommitTransaction(MakeTransactionRef(tx), nullptr, g_connman.get(), extras);
    CValidationState& state = res.state;
    if (state.IsInvalid()) {
        std::string debugMsg = state.GetDebugMessage();
        return {false, debugMsg.empty() ? state.GetRejectReason() :
                strprintf("%s: %s", state.GetRejectReason(), debugMsg)};
    }

    return {true};
}

#endif

Optional<CKeyID> ParsePubKeyIDFromAddress(const std::string& strAddress, std::string& strError)
{
    bool isStaking{false}, isShield{false};
    const CWDestination& cwdest = Standard::DecodeDestination(strAddress, isStaking, isShield);
    if (isStaking) {
        strError = "cold staking addresses not supported";
        return nullopt;
    }
    if (isShield) {
        strError = "shield addresses not supported";
        return nullopt;
    }
    const CKeyID* keyID = boost::get<CKeyID>(Standard::GetTransparentDestination(cwdest));
    if (!keyID) {
        strError = strprintf("invalid PIVX address %s", strAddress);
        return nullopt;
    }
    return *keyID;
}
