// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "budget/finalizedbudgetvote.h"

#include "net.h"

CFinalizedBudgetVote::CFinalizedBudgetVote() :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(),
        nBudgetHash(),
        nTime(0)
{ }

CFinalizedBudgetVote::CFinalizedBudgetVote(const CTxIn& vinIn, const uint256& nBudgetHashIn) :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(vinIn),
        nBudgetHash(nBudgetHashIn)
{
    nTime = GetAdjustedTime();
}

void CFinalizedBudgetVote::Relay() const
{
    CInv inv(MSG_BUDGET_FINALIZED_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

uint256 CFinalizedBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nBudgetHash;
    ss << nTime;
    return ss.GetHash();
}

UniValue CFinalizedBudgetVote::ToJSON() const
{
    UniValue bObj(UniValue::VOBJ);
    bObj.pushKV("nHash", GetHash().ToString());
    bObj.pushKV("nTime", (int64_t) nTime);
    bObj.pushKV("fValid", fValid);
    return bObj;
}

std::string CFinalizedBudgetVote::GetStrMessage() const
{
    return strprintf("%s %s %d", vin.prevout.ToStringShort(), nBudgetHash.ToString(), nTime);
}
