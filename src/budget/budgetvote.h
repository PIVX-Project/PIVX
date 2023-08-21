// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BUDGET_VOTE_H
#define BUDGET_VOTE_H

#include "messagesigner.h"
#include "primitives/transaction.h"

#include <univalue.h>

//
// CBudgetVote - Allow a gamemaster node to vote and broadcast throughout the network
//

class CBudgetVote : public CSignedMessage
{
public:
    enum VoteDirection : uint32_t {
        VOTE_ABSTAIN = 0,
        VOTE_YES = 1,
        VOTE_NO = 2
    };

private:
    bool fValid;  //if the vote is currently valid / counted
    bool fSynced; //if we've sent this to our peers
    uint256 nProposalHash;
    VoteDirection nVote;
    int64_t nTime;
    CTxIn vin;

public:
    CBudgetVote();
    CBudgetVote(const CTxIn& vin, const uint256& nProposalHash, VoteDirection nVoteIn);

    void Relay() const;

    std::string GetVoteString() const
    {
        std::string ret = "ABSTAIN";
        if (nVote == VOTE_YES) ret = "YES";
        if (nVote == VOTE_NO) ret = "NO";
        return ret;
    }

    uint256 GetHash() const;

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override { return GetHash(); }
    std::string GetStrMessage() const override;
    CTxIn GetVin() const { return vin; };

    UniValue ToJSON() const;

    VoteDirection GetDirection() const { return nVote; }
    uint256 GetProposalHash() const { return nProposalHash; }
    int64_t GetTime() const { return nTime; }
    bool IsSynced() const { return fSynced; }
    bool IsValid() const { return fValid; }

    void SetSynced(bool _fSynced) { fSynced = _fSynced; }
    void SetTime(const int64_t& _nTime) { nTime = _nTime; }
    void SetValid(bool _fValid) { fValid = _fValid; }

    SERIALIZE_METHODS(CBudgetVote, obj) { READWRITE(obj.vin, obj.nProposalHash, Using<CustomUintFormatter<4>>(obj.nVote), obj.nTime, obj.vchSig, obj.nMessVersion); }
};

#endif // BUDGET_VOTE_H
