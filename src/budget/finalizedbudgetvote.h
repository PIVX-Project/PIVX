// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FINALIZED_BUDGET_VOTE_H
#define FINALIZED_BUDGET_VOTE_H

#include "messagesigner.h"
#include "primitives/transaction.h"

#include <univalue.h>


//
// CFinalizedBudgetVote - Allow a masternode node to vote and broadcast throughout the network
//

class CFinalizedBudgetVote : public CSignedMessage
{
private:
    bool fValid;  //if the vote is currently valid / counted
    bool fSynced; //if we've sent this to our peers
    CTxIn vin;
    uint256 nBudgetHash;
    int64_t nTime;

public:
    CFinalizedBudgetVote();
    CFinalizedBudgetVote(const CTxIn& vinIn, const uint256& nBudgetHashIn);

    void Relay() const;
    uint256 GetHash() const;

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override { return GetHash(); }
    std::string GetStrMessage() const override;
    CTxIn GetVin() const { return vin; };

    UniValue ToJSON() const;

    uint256 GetBudgetHash() const { return nBudgetHash; }
    int64_t GetTime() const { return nTime; }
    bool IsSynced() const { return fSynced; }
    bool IsValid() const { return fValid; }

    void SetSynced(bool _fSynced) { fSynced = _fSynced; }
    void SetTime(const int64_t& _nTime) { nTime = _nTime; }
    void SetValid(bool _fValid) { fValid = _fValid; }

    SERIALIZE_METHODS(CFinalizedBudgetVote, obj) { READWRITE(obj.vin, obj.nBudgetHash, obj.nTime, obj.vchSig, obj.nMessVersion); }
};

#endif // FINALIZED_BUDGET_VOTE_H
