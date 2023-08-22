// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMEMASTER_PAYMENTS_H
#define GAMEMASTER_PAYMENTS_H

#include "key.h"
#include "gamemaster.h"
#include "validationinterface.h"


extern RecursiveMutex cs_vecPayments;
extern RecursiveMutex cs_mapGamemasterBlocks;
extern RecursiveMutex cs_mapGamemasterPayeeVotes;

class CGamemasterPayments;
class CGamemasterPaymentWinner;
class CGamemasterBlockPayees;
class CValidationState;

extern CGamemasterPayments gamemasterPayments;

#define GMPAYMENTS_SIGNATURES_REQUIRED 6
#define GMPAYMENTS_SIGNATURES_TOTAL 10

bool IsBlockPayeeValid(const CBlock& block, const CBlockIndex* pindexPrev);
std::string GetRequiredPaymentsString(int nBlockHeight);
bool IsBlockValueValid(int nHeight, CAmount& nExpectedValue, CAmount nMinted, CAmount& nBudgetAmt);
void FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake);

/**
 * Check coinbase output value for blocks after v6.0 enforcement.
 * It must pay the gamemaster for regular blocks and a proposal during superblocks.
 */
bool IsCoinbaseValueValid(const CTransactionRef& tx, CAmount nBudgetAmt, CValidationState& _state);

void DumpGamemasterPayments();

/** Save Gamemaster Payment Data (gmpayments.dat)
 */
class CGamemasterPaymentDB
{
private:
    fs::path pathDB;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CGamemasterPaymentDB();
    bool Write(const CGamemasterPayments& objToSave);
    ReadResult Read(CGamemasterPayments& objToLoad);
};

class CGamemasterPayee
{
public:
    CScript scriptPubKey;
    int nVotes;

    CGamemasterPayee()
    {
        scriptPubKey = CScript();
        nVotes = 0;
    }

    CGamemasterPayee(CScript payee, int nVotesIn)
    {
        scriptPubKey = payee;
        nVotes = nVotesIn;
    }

    SERIALIZE_METHODS(CGamemasterPayee, obj) { READWRITE(obj.scriptPubKey, obj.nVotes); }
};

// Keep track of votes for payees from gamemasters
class CGamemasterBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CGamemasterPayee> vecPayments;

    CGamemasterBlockPayees()
    {
        nBlockHeight = 0;
        vecPayments.clear();
    }
    CGamemasterBlockPayees(int nBlockHeightIn)
    {
        nBlockHeight = nBlockHeightIn;
        vecPayments.clear();
    }

    void AddPayee(const CScript& payeeIn, int nIncrement)
    {
        LOCK(cs_vecPayments);

        for (CGamemasterPayee& payee : vecPayments) {
            if (payee.scriptPubKey == payeeIn) {
                payee.nVotes += nIncrement;
                return;
            }
        }

        CGamemasterPayee c(payeeIn, nIncrement);
        vecPayments.push_back(c);
    }

    bool GetPayee(CScript& payee) const
    {
        LOCK(cs_vecPayments);

        int nVotes = -1;
        for (const CGamemasterPayee& p : vecPayments) {
            if (p.nVotes > nVotes) {
                payee = p.scriptPubKey;
                nVotes = p.nVotes;
            }
        }

        return (nVotes > -1);
    }

    bool HasPayeeWithVotes(const CScript& payee, int nVotesReq)
    {
        LOCK(cs_vecPayments);

        for (CGamemasterPayee& p : vecPayments) {
            if (p.nVotes >= nVotesReq && p.scriptPubKey == payee) return true;
        }

        return false;
    }

    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    std::string GetRequiredPaymentsString();

    SERIALIZE_METHODS(CGamemasterBlockPayees, obj) { READWRITE(obj.nBlockHeight, obj.vecPayments); }
};

// for storing the winning payments
class CGamemasterPaymentWinner : public CSignedMessage
{
public:
    CTxIn vinGamemaster;
    int nBlockHeight;
    CScript payee;

    CGamemasterPaymentWinner() :
        CSignedMessage(),
        vinGamemaster(),
        nBlockHeight(0),
        payee()
    {}

    CGamemasterPaymentWinner(const CTxIn& vinIn, int nHeight):
        CSignedMessage(),
        vinGamemaster(vinIn),
        nBlockHeight(nHeight),
        payee()
    {}

    uint256 GetHash() const;

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override { return GetHash(); }
    std::string GetStrMessage() const override;
    CTxIn GetVin() const { return vinGamemaster; };

    bool IsValid(CNode* pnode, CValidationState& state, int chainHeight);
    void Relay();

    void AddPayee(const CScript& payeeIn)
    {
        payee = payeeIn;
    }

    SERIALIZE_METHODS(CGamemasterPaymentWinner, obj) { READWRITE(obj.vinGamemaster, obj.nBlockHeight, obj.payee, obj.vchSig, obj.nMessVersion); }

    std::string ToString()
    {
        std::string ret = "";
        ret += vinGamemaster.ToString();
        ret += ", " + std::to_string(nBlockHeight);
        ret += ", " + HexStr(payee);
        ret += ", " + std::to_string((int)vchSig.size());
        return ret;
    }
};

//
// Gamemaster Payments Class
// Keeps track of who should get paid for which blocks
//

class CGamemasterPayments : public CValidationInterface
{
private:
    int nLastBlockHeight;

public:
    std::map<uint256, CGamemasterPaymentWinner> mapGamemasterPayeeVotes;
    std::map<int, CGamemasterBlockPayees> mapGamemasterBlocks;

    CGamemasterPayments()
    {
        nLastBlockHeight = 0;
    }

    void Clear()
    {
        LOCK2(cs_mapGamemasterBlocks, cs_mapGamemasterPayeeVotes);
        mapGamemasterBlocks.clear();
        mapGamemasterPayeeVotes.clear();
    }

    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;

    void AddWinningGamemaster(CGamemasterPaymentWinner& winner);
    void ProcessBlock(int nBlockHeight);

    void Sync(CNode* node, int nCountNeeded);
    void CleanPaymentList(int gmCount, int nHeight);

    // get the gamemaster payment outs for block built on top of pindexPrev
    bool GetGamemasterTxOuts(const CBlockIndex* pindexPrev, std::vector<CTxOut>& voutGamemasterPaymentsRet) const;

    // can be removed after transition to DGM
    bool GetLegacyGamemasterTxOut(int nHeight, std::vector<CTxOut>& voutGamemasterPaymentsRet) const;
    bool GetBlockPayee(int nBlockHeight, CScript& payee) const;

    bool IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev);
    bool IsScheduled(const CGamemaster& gm, int nNotBlockHeight);

    bool ProcessGMWinner(CGamemasterPaymentWinner& winner, CNode* pfrom, CValidationState& state);
    bool ProcessMessageGamemasterPayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, CValidationState& state);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake) const;
    std::string ToString() const;

    SERIALIZE_METHODS(CGamemasterPayments, obj) { READWRITE(obj.mapGamemasterPayeeVotes, obj.mapGamemasterBlocks); }

private:
    // keep track of last voted height for gmw signers
    std::map<COutPoint, int> mapGamemastersLastVote; //prevout, nBlockHeight

    bool CanVote(const COutPoint& outGamemaster, int nBlockHeight) const;
    void RecordWinnerVote(const COutPoint& outGamemaster, int nBlockHeight);
};


#endif
