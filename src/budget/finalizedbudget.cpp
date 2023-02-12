// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "budget/finalizedbudget.h"

#include "masternodeman.h"
#include "validation.h"

CFinalizedBudget::CFinalizedBudget() :
        fAutoChecked(false),
        fValid(true),
        strInvalid(),
        mapVotes(),
        strBudgetName(""),
        nBlockStart(0),
        vecBudgetPayments(),
        nFeeTXHash(UINT256_ZERO),
        strProposals(""),
        nTime(0)
{ }

CFinalizedBudget::CFinalizedBudget(const std::string& name,
                                   int blockstart,
                                   const std::vector<CTxBudgetPayment>& vecBudgetPaymentsIn,
                                   const uint256& nfeetxhash):
        fAutoChecked(false),
        fValid(true),
        strInvalid(),
        mapVotes(),
        strBudgetName(name),
        nBlockStart(blockstart),
        vecBudgetPayments(vecBudgetPaymentsIn),
        nFeeTXHash(nfeetxhash),
        strProposals(""),
        nTime(0)
{ }

bool CFinalizedBudget::ParseBroadcast(CDataStream& broadcast)
{
    *this = CFinalizedBudget();
    try {
        broadcast >> LIMITED_STRING(strBudgetName, 20);
        broadcast >> nBlockStart;
        broadcast >> vecBudgetPayments;
        broadcast >> nFeeTXHash;
    } catch (std::exception& e) {
        return error("Unable to deserialize finalized budget broadcast: %s", e.what());
    }
    return true;
}

bool CFinalizedBudget::AddOrUpdateVote(const CFinalizedBudgetVote& vote, std::string& strError)
{
    const COutPoint& mnId = vote.GetVin().prevout;
    const int64_t voteTime = vote.GetTime();
    std::string strAction = "New vote inserted:";

    if (mapVotes.count(mnId)) {
        const int64_t oldTime = mapVotes[mnId].GetTime();
        if (oldTime > voteTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        if (voteTime - oldTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n",
                    vote.GetHash().ToString(), voteTime - oldTime, BUDGET_VOTE_UPDATE_MIN);
            LogPrint(BCLog::MNBUDGET, "%s: %s\n", __func__, strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    mapVotes[mnId] = vote;
    LogPrint(BCLog::MNBUDGET, "%s: %s %s\n", __func__, strAction.c_str(), vote.GetHash().ToString().c_str());
    return true;
}

void CFinalizedBudget::SetSynced(bool synced)
{
    for (auto& it: mapVotes) {
        CFinalizedBudgetVote& vote = it.second;
        if (synced) {
            if (vote.IsValid()) vote.SetSynced(true);
        } else {
            vote.SetSynced(false);
        }
    }
}

bool CFinalizedBudget::CheckProposals(const std::map<uint256, CBudgetProposal>& mapWinningProposals) const
{
    if (mapWinningProposals.empty()) {
        LogPrint(BCLog::MNBUDGET,"%s: No Budget-Proposals found, aborting\n", __func__);
        return false;
    }

    if (mapWinningProposals.size() != vecBudgetPayments.size()) {
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposal length (%ld) doesn't match Budget-Payment length (%ld).\n",
                __func__, mapWinningProposals.size(), vecBudgetPayments.size());
        return false;
    }

    for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - nProp %d %s\n", __func__, i, vecBudgetPayments[i].nProposalHash.ToString());
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - Payee %d %s\n", __func__, i, HexStr(vecBudgetPayments[i].payee));
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Payments - nAmount %d %lli\n", __func__, i, vecBudgetPayments[i].nAmount);
    }

    for (const auto& it: mapWinningProposals) {
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - nProp %s\n", __func__, (it.first).ToString());
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - Payee %s\n", __func__, HexStr((it.second).GetPayee()));
        LogPrint(BCLog::MNBUDGET,"%s: Budget-Proposals - nAmount %lli\n", __func__, (it.second).GetAmount());
    }

    for (const CTxBudgetPayment& p : vecBudgetPayments) {
        const auto& it = mapWinningProposals.find(p.nProposalHash);
        if (it == mapWinningProposals.end()) {
            LogPrint(BCLog::MNBUDGET,"%s: Proposal %s not found\n", __func__, p.nProposalHash.ToString());
            return false;
        }

        const CBudgetProposal& prop = it->second;
        if (p.payee != prop.GetPayee()) {
            LogPrint(BCLog::MNBUDGET,"%s: payee doesn't match %s != %s\n", __func__, HexStr(p.payee), HexStr(prop.GetPayee()));
            return false;
        }

        if (p.nAmount != prop.GetAmount()) {
            LogPrint(BCLog::MNBUDGET,"%s: payee amount doesn't match %lli != %lli\n", __func__, p.nAmount, prop.GetAmount());
            return false;
        }
    }

    LogPrint(BCLog::MNBUDGET,"%s: Finalized Budget Matches! Submitting Vote.\n", __func__);
    return true;
}

CAmount CFinalizedBudget::GetTotalPayout() const
{
    CAmount ret = 0;

    for (auto & vecBudgetPayment : vecBudgetPayments) {
        ret += vecBudgetPayment.nAmount;
    }

    return ret;
}

std::vector<uint256> CFinalizedBudget::GetProposalsHashes() const
{
    std::vector<uint256> vHashes;
    for (const CTxBudgetPayment& budgetPayment : vecBudgetPayments) {
        vHashes.push_back(budgetPayment.nProposalHash);
    }
    return vHashes;
}

void CFinalizedBudget::SyncVotes(CNode* pfrom, bool fPartial, int& nInvCount) const
{
    for (const auto& it: mapVotes) {
        const CFinalizedBudgetVote& vote = it.second;
        if (vote.IsValid() && (!fPartial || !vote.IsSynced())) {
            pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED_VOTE, vote.GetHash()));
            nInvCount++;
        }
    }
}

bool CFinalizedBudget::CheckStartEnd()
{
    if (nBlockStart == 0) {
        strInvalid = "Invalid BlockStart == 0";
        return false;
    }

    // Must be the correct block for payment to happen (once a month)
    if (nBlockStart % Params().GetConsensus().nBudgetCycleBlocks != 0) {
        strInvalid = "Invalid BlockStart";
        return false;
    }

    // The following 2 checks check the same (basically if vecBudgetPayments.size() > 100)
    if (GetBlockEnd() - nBlockStart + 1 > (int) MAX_PROPOSALS_PER_CYCLE) {
        strInvalid = "Invalid BlockEnd";
        return false;
    }
    if ((int)vecBudgetPayments.size() > (int) MAX_PROPOSALS_PER_CYCLE) {
        strInvalid = "Invalid budget payments count (too many)";
        return false;
    }

    return true;
}

bool CFinalizedBudget::CheckAmount(const CAmount& nTotalBudget)
{
    // Can only pay out 10% of the possible coins (min value of coins)
    if (GetTotalPayout() > nTotalBudget) {
        strInvalid = "Invalid Payout (more than max)";
        return false;
    }

    return true;
}

bool CFinalizedBudget::CheckName()
{
    if (strBudgetName == "") {
        strInvalid = "Invalid Budget Name";
        return false;
    }

    return true;
}

bool CFinalizedBudget::updateExpired(int nCurrentHeight)
{
    // Remove finalized budgets 2 * MAX_PROPOSALS_PER_CYCLE blocks after their end
    const int nBlockEnd = GetBlockEnd();
    if (nCurrentHeight >= nBlockEnd + 2 * (int) MAX_PROPOSALS_PER_CYCLE) {
        strInvalid = strprintf("(ends at block %ld) too old and obsolete (current %ld)", nBlockEnd, nCurrentHeight);
        return true;
    }

    return false;
}

bool CFinalizedBudget::IsWellFormed(const CAmount& nTotalBudget)
{
    return CheckStartEnd() && CheckAmount(nTotalBudget) && CheckName();
}

bool CFinalizedBudget::UpdateValid(int nCurrentHeight)
{
    fValid = false;

    if (updateExpired(nCurrentHeight)) {
        return false;
    }

    fValid = true;
    strInvalid.clear();
    return true;
}

int CFinalizedBudget::GetVoteCount() const
{
    int ret = 0;
    for (const auto& it : mapVotes) {
        if (it.second.IsValid()) {
            ret++;
        }
    }
    return ret;
}

std::vector<uint256> CFinalizedBudget::GetVotesHashes() const
{
    std::vector<uint256> vRet;
    for (const auto& it: mapVotes) {
        vRet.push_back(it.second.GetHash());
    }
    return vRet;
}

bool CFinalizedBudget::IsPaidAlready(const uint256& nProposalHash, const uint256& nBlockHash, int nBlockHeight) const
{
    // Remove budget-payments from former/future payment cycles
    int nPaidBlockHeight = 0;
    uint256 nOldProposalHash;

    for(auto it = mapPayment_History.begin(); it != mapPayment_History.end(); /* No incrementation needed */ ) {
        nPaidBlockHeight = (*it).second.second;
        if((nPaidBlockHeight < GetBlockStart()) || (nPaidBlockHeight > GetBlockEnd())) {
            nOldProposalHash = (*it).first;
            LogPrint(BCLog::MNBUDGET, "%s: Budget Proposal %s, Block %d from old cycle deleted\n",
                    __func__, nOldProposalHash.ToString().c_str(), nPaidBlockHeight);
            it = mapPayment_History.erase(it);
        } else {
            ++it;
        }
    }

    // Now that we only have payments from the current payment cycle check if this budget was paid already
    if(mapPayment_History.count(nProposalHash) == 0) {
        // New proposal payment, insert into map for checks with later blocks from this cycle
        mapPayment_History.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(nProposalHash),
                                   std::forward_as_tuple(nBlockHash, nBlockHeight));
        LogPrint(BCLog::MNBUDGET, "%s: Budget Proposal %s, Block %d (%s) added to payment history (size=%d)\n",
                __func__, nProposalHash.ToString(), nBlockHeight, nBlockHash.ToString(), mapPayment_History.size());
        return false;
    }
    // This budget payment was already checked/paid
    const uint256& nPaidBlockHash = mapPayment_History.at(nProposalHash).first;

    // If we are checking a different block, and the paid one is on chain
    // -> reject transaction so it gets paid to a masternode instead
    if (nBlockHash != nPaidBlockHash) {
        LOCK(cs_main);
        CBlockIndex* pindex = LookupBlockIndex(nPaidBlockHash);
        return pindex && chainActive.Contains(pindex);
    }

    // Re-checking same block. Not a double payment.
    return false;
}

TrxValidationStatus CFinalizedBudget::IsTransactionValid(const CTransaction& txNew, const uint256& nBlockHash, int nBlockHeight) const
{
    const int nBlockEnd = GetBlockEnd();
    if (nBlockHeight > nBlockEnd) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid block - height: %d end: %d\n", __func__, nBlockHeight, nBlockEnd);
        return TrxValidationStatus::InValid;
    }
    if (nBlockHeight < nBlockStart) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid block - height: %d start: %d\n", __func__, nBlockHeight, nBlockStart);
        return TrxValidationStatus::InValid;
    }

    const int nCurrentBudgetPayment = nBlockHeight - nBlockStart;
    if (nCurrentBudgetPayment > (int)vecBudgetPayments.size() - 1) {
        LogPrint(BCLog::MNBUDGET,"%s: Invalid last block - current budget payment: %d of %d\n",
                __func__, nCurrentBudgetPayment + 1, (int)vecBudgetPayments.size());
        return TrxValidationStatus::InValid;
    }

    // Check if this proposal was paid already. If so, pay a masternode instead
    if(IsPaidAlready(vecBudgetPayments[nCurrentBudgetPayment].nProposalHash, nBlockHash, nBlockHeight)) {
        LogPrint(BCLog::MNBUDGET,"%s: Double Budget Payment of %d for proposal %d detected. Paying a masternode instead.\n",
                __func__, vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
        // No matter what we've found before, stop all checks here. In future releases there might be more than one budget payment
        // per block, so even if the first one was not paid yet this one disables all budget payments for this block.
        return TrxValidationStatus::DoublePayment;
    }

    // Search the payment
    const CScript& scriptExpected = vecBudgetPayments[nCurrentBudgetPayment].payee;
    const CAmount& amountExpected = vecBudgetPayments[nCurrentBudgetPayment].nAmount;
    // Budget payment is usually the last output of coinstake txes, iterate backwords
    for (auto out = txNew.vout.rbegin(); out != txNew.vout.rend(); ++out) {
        LogPrint(BCLog::MNBUDGET,"%s: nCurrentBudgetPayment=%d, payee=%s == out.scriptPubKey=%s, amount=%ld == out.nValue=%ld\n",
                __func__, nCurrentBudgetPayment, HexStr(scriptExpected), HexStr(out->scriptPubKey), amountExpected, out->nValue);
        if (scriptExpected == out->scriptPubKey && amountExpected == out->nValue) {
            // payment found
            LogPrint(BCLog::MNBUDGET,"%s: Found valid Budget Payment of %d for proposal %d\n",
                    __func__, amountExpected, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
            return TrxValidationStatus::Valid;
        }
    }

    // payment not found
    CTxDestination address1;
    ExtractDestination(scriptExpected, address1);
    LogPrint(BCLog::MNBUDGET,"%s: Missing required payment - %s: %d c: %d\n",
            __func__, EncodeDestination(address1), amountExpected, nCurrentBudgetPayment);
    return TrxValidationStatus::InValid;
}

bool CFinalizedBudget::GetBudgetPaymentByBlock(int64_t nBlockHeight, CTxBudgetPayment& payment) const
{
    int i = nBlockHeight - GetBlockStart();
    if (i < 0) return false;
    if (i > (int)vecBudgetPayments.size() - 1) return false;
    payment = vecBudgetPayments[i];
    return true;
}

bool CFinalizedBudget::GetPayeeAndAmount(int64_t nBlockHeight, CScript& payee, CAmount& nAmount) const
{
    int i = nBlockHeight - GetBlockStart();
    if (i < 0) return false;
    if (i > (int)vecBudgetPayments.size() - 1) return false;
    payee = vecBudgetPayments[i].payee;
    nAmount = vecBudgetPayments[i].nAmount;
    return true;
}

// return broadcast serialization
CDataStream CFinalizedBudget::GetBroadcast() const
{
    CDataStream broadcast(SER_NETWORK, PROTOCOL_VERSION);
    broadcast.reserve(1000);
    broadcast << LIMITED_STRING(strBudgetName, 20);
    broadcast << nBlockStart;
    broadcast << vecBudgetPayments;
    broadcast << nFeeTXHash;
    return broadcast;
}


bool CFinalizedBudget::operator>(const CFinalizedBudget& other) const
{
    const int count = GetVoteCount();
    const int otherCount = other.GetVoteCount();

    if (count == otherCount) return UintToArith256(GetFeeTXHash()) > UintToArith256(other.GetFeeTXHash());

    return count > otherCount;
}

void CFinalizedBudget::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED, GetHash());
    g_connman->RelayInv(inv);
}
