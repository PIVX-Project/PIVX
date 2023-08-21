// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "budget/budgetmanager.h"

#include "consensus/validation.h"
#include "evo/deterministicgms.h"
#include "gamemasterman.h"
#include "netmessagemaker.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "tiertwo/netfulfilledman.h"
#include "util/validation.h"
#include "validation.h"   // GetTransaction, cs_main

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // future: use interface instead.
#endif


#define BUDGET_ORPHAN_VOTES_CLEANUP_SECONDS (60 * 60) // One hour.
// Request type used in the net requests manager to block peers asking budget sync too often
static const std::string BUDGET_SYNC_REQUEST_RECV = "budget-sync-recv";

CBudgetManager g_budgetman;

// Used to check both proposals and finalized-budgets collateral txes
bool CheckCollateral(const uint256& nTxCollateralHash, const uint256& nExpectedHash, std::string& strError, int64_t& nTime, int nCurrentHeight, bool fBudgetFinalization);

void CBudgetManager::ReloadMapSeen()
{
    const auto reloadSeenMap = [](auto& mutex1, auto& mutex2, const auto& mapBudgets, auto& mapSeen, auto& mapOrphans) {
        LOCK2(mutex1, mutex2);
        mapSeen.clear();
        mapOrphans.clear();
        for (const auto& b : mapBudgets) {
            for (const auto& it : b.second.mapVotes) {
                const auto& vote = it.second;
                if (vote.IsValid()) {
                    mapSeen.emplace(vote.GetHash(), vote);
                }
            }
        }
    };

    reloadSeenMap(cs_proposals, cs_votes, mapProposals, mapSeenProposalVotes, mapOrphanProposalVotes);
    reloadSeenMap(cs_budgets, cs_finalizedvotes, mapFinalizedBudgets, mapSeenFinalizedBudgetVotes, mapOrphanFinalizedBudgetVotes);
}

void CBudgetManager::CheckOrphanVotes()
{
    {
        LOCK2(cs_proposals, cs_votes);
        for (auto itOrphanVotes = mapOrphanProposalVotes.begin(); itOrphanVotes != mapOrphanProposalVotes.end();) {
            auto itProposal = mapProposals.find(itOrphanVotes->first);
            if (itProposal != mapProposals.end()) {
                // Proposal found.
                CBudgetProposal* bp = &(itProposal->second);
                // Try to add orphan votes
                for (const CBudgetVote& vote : itOrphanVotes->second.first) {
                    std::string strError;
                    if (!bp->AddOrUpdateVote(vote, strError)) {
                        LogPrint(BCLog::GMBUDGET, "Unable to add orphan vote for proposal: %s\n", strError);
                    }
                }
                // Remove entry from the map
                itOrphanVotes = mapOrphanProposalVotes.erase(itOrphanVotes);
            } else {
                ++itOrphanVotes;
            }
        }
    }

    {
        LOCK2(cs_budgets, cs_finalizedvotes);
        for (auto itOrphanVotes = mapOrphanFinalizedBudgetVotes.begin(); itOrphanVotes != mapOrphanFinalizedBudgetVotes.end();) {
            auto itFinalBudget = mapFinalizedBudgets.find(itOrphanVotes->first);
            if (itFinalBudget != mapFinalizedBudgets.end()) {
                // Finalized budget found.
                CFinalizedBudget* fb = &(itFinalBudget->second);
                // Try to add orphan votes
                for (const CFinalizedBudgetVote& vote : itOrphanVotes->second.first) {
                    std::string strError;
                    if (!fb->AddOrUpdateVote(vote, strError)) {
                        LogPrint(BCLog::GMBUDGET, "Unable to add orphan vote for final budget: %s\n", strError);
                    }
                }
                // Remove entry from the map
                itOrphanVotes = mapOrphanFinalizedBudgetVotes.erase(itOrphanVotes);
            } else {
                ++itOrphanVotes;
            }
        }
    }

    LogPrint(BCLog::GMBUDGET,"%s: Done\n", __func__);
}

uint256 CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final budget was submitted last time
    int nCurrentHeight = GetBestHeight();

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = nCurrentHeight - nCurrentHeight % nBlocksPerCycle + nBlocksPerCycle;
    if (nSubmittedHeight >= nBlockStart){
        LogPrint(BCLog::GMBUDGET,"%s: nSubmittedHeight(=%ld) < nBlockStart(=%ld) condition not fulfilled.\n",
                __func__, nSubmittedHeight, nBlockStart);
        return UINT256_ZERO;
    }

     // Submit final budget during the last 2 days (2880 blocks) before payment for Mainnet, about 9 minutes (9 blocks) for Testnet
    int finalizationWindow = ((nBlocksPerCycle / 30) * 2);

    if (Params().IsTestnet()) {
        // NOTE: 9 blocks for testnet is way to short to have any gamemaster submit an automatic vote on the finalized(!) budget,
        //       because those votes are only submitted/relayed once every 56 blocks in CFinalizedBudget::AutoCheck()

        finalizationWindow = 64; // 56 + 4 finalization confirmations + 4 minutes buffer for propagation
    }

    int nFinalizationStart = nBlockStart - finalizationWindow;

    int nOffsetToStart = nFinalizationStart - nCurrentHeight;

    if (nBlockStart - nCurrentHeight > finalizationWindow) {
        LogPrint(BCLog::GMBUDGET,"%s: Too early for finalization. Current block is %ld, next Superblock is %ld.\n", __func__, nCurrentHeight, nBlockStart);
        LogPrint(BCLog::GMBUDGET,"%s: First possible block for finalization: %ld. Last possible block for finalization: %ld. "
                "You have to wait for %ld block(s) until Budget finalization will be possible\n", __func__, nFinalizationStart, nBlockStart, nOffsetToStart);
        return UINT256_ZERO;
    }

    std::vector<CBudgetProposal> vBudgetProposals = GetBudget();
    std::string strBudgetName = "main";
    std::vector<CTxBudgetPayment> vecTxBudgetPayments;

    for (const auto& p : vBudgetProposals) {
        CTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = p.GetHash();
        txBudgetPayment.payee = p.GetPayee();
        txBudgetPayment.nAmount = p.GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.size() < 1) {
        LogPrint(BCLog::GMBUDGET,"%s: Found No Proposals For Period\n", __func__);
        return UINT256_ZERO;
    }

    CFinalizedBudget tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, UINT256_ZERO);
    const uint256& budgetHash = tempBudget.GetHash();
    if (HaveFinalizedBudget(budgetHash)) {
        LogPrint(BCLog::GMBUDGET,"%s: Budget already exists - %s\n", __func__, budgetHash.ToString());
        nSubmittedHeight = nCurrentHeight;
        return UINT256_ZERO;
    }

    // See if collateral tx exists
    if (!mapUnconfirmedFeeTx.count(budgetHash)) {
        // create the collateral tx, send it to the network and return
        CTransactionRef wtx;
        // Get our change address
        if (vpwallets.empty() || !vpwallets[0]) {
            LogPrint(BCLog::GMBUDGET,"%s: Wallet not found\n", __func__);
            return UINT256_ZERO;
        }
        CReserveKey keyChange(vpwallets[0]);
        if (!vpwallets[0]->CreateBudgetFeeTX(wtx, budgetHash, keyChange, BUDGET_FEE_TX)) {
            LogPrint(BCLog::GMBUDGET,"%s: Can't make collateral transaction\n", __func__);
            return UINT256_ZERO;
        }
        // Send the tx to the network
        const CWallet::CommitResult& res = vpwallets[0]->CommitTransaction(wtx, keyChange, g_connman.get());
        if (res.status == CWallet::CommitStatus::OK) {
            const uint256& collateraltxid = wtx->GetHash();
            mapUnconfirmedFeeTx.emplace(budgetHash, collateraltxid);
            LogPrint(BCLog::GMBUDGET,"%s: Collateral sent. txid: %s\n", __func__, collateraltxid.ToString());
            return budgetHash;
        }
        return UINT256_ZERO;
    }

    // Collateral tx already exists, see if it's mature enough.
    CFinalizedBudget fb(strBudgetName, nBlockStart, vecTxBudgetPayments, mapUnconfirmedFeeTx.at(budgetHash));
    if (!AddFinalizedBudget(fb)) {
        return UINT256_ZERO;
    }
    fb.Relay();
    nSubmittedHeight = nCurrentHeight;
    LogPrint(BCLog::GMBUDGET,"%s: Done! %s\n", __func__, budgetHash.ToString());
    return budgetHash;
}

void CBudgetManager::SetBudgetProposalsStr(CFinalizedBudget& finalizedBudget) const
{
    const std::vector<uint256>& vHashes = finalizedBudget.GetProposalsHashes();
    std::string strProposals = "";
    {
        LOCK(cs_proposals);
        for (const uint256& hash: vHashes) {
            const std::string token = (mapProposals.count(hash) ? mapProposals.at(hash).GetName() : hash.ToString());
            strProposals += (strProposals == "" ? "" : ", ") + token;
        }
    }
    finalizedBudget.SetProposalsStr(strProposals);
}

std::string CBudgetManager::GetFinalizedBudgetStatus(const uint256& nHash) const
{
    CFinalizedBudget fb;
    if (!GetFinalizedBudget(nHash, fb))
        return strprintf("ERROR: cannot find finalized budget %s\n", nHash.ToString());

    std::string retBadHashes = "";
    std::string retBadPayeeOrAmount = "";
    int nBlockStart = fb.GetBlockStart();
    int nBlockEnd = fb.GetBlockEnd();

    for (int nBlockHeight = nBlockStart; nBlockHeight <= nBlockEnd; nBlockHeight++) {
        CTxBudgetPayment budgetPayment;
        if (!fb.GetBudgetPaymentByBlock(nBlockHeight, budgetPayment)) {
            LogPrint(BCLog::GMBUDGET,"%s: Couldn't find budget payment for block %lld\n", __func__, nBlockHeight);
            continue;
        }

        CBudgetProposal bp;
        if (!GetProposal(budgetPayment.nProposalHash, bp)) {
            retBadHashes += (retBadHashes == "" ? "" : ", ") + budgetPayment.nProposalHash.ToString();
            continue;
        }

        if (bp.GetPayee() != budgetPayment.payee || bp.GetAmount() != budgetPayment.nAmount) {
            retBadPayeeOrAmount += (retBadPayeeOrAmount == "" ? "" : ", ") + budgetPayment.nProposalHash.ToString();
        }
    }

    if (retBadHashes == "" && retBadPayeeOrAmount == "") return "OK";

    if (retBadHashes != "") retBadHashes = "Unknown proposal(s) hash! Check this proposal(s) before voting: " + retBadHashes;
    if (retBadPayeeOrAmount != "") retBadPayeeOrAmount = "Budget payee/nAmount doesn't match our proposal(s)! "+ retBadPayeeOrAmount;

    return retBadHashes + " -- " + retBadPayeeOrAmount;
}

bool CBudgetManager::AddFinalizedBudget(CFinalizedBudget& finalizedBudget, CNode* pfrom)
{
    AssertLockNotHeld(cs_budgets);    // need to lock cs_main here (CheckCollateral)
    const uint256& nHash = finalizedBudget.GetHash();

    if (WITH_LOCK(cs_budgets, return mapFinalizedBudgets.count(nHash))) {
        LogPrint(BCLog::GMBUDGET,"%s: finalized budget %s already added\n", __func__, nHash.ToString());
        return false;
    }

    if (!finalizedBudget.IsWellFormed(GetTotalBudget(finalizedBudget.GetBlockStart()))) {
        LogPrint(BCLog::GMBUDGET,"%s: invalid finalized budget: %s %s\n", __func__, nHash.ToString(), finalizedBudget.IsInvalidLogStr());
        return false;
    }

    std::string strError;
    int nCurrentHeight = GetBestHeight();
    const uint256& feeTxId = finalizedBudget.GetFeeTXHash();
    if (!CheckCollateral(feeTxId, nHash, strError, finalizedBudget.nTime, nCurrentHeight, true)) {
        LogPrint(BCLog::GMBUDGET,"%s: invalid finalized budget (%s) collateral id=%s - %s\n",
                __func__, nHash.ToString(), feeTxId.ToString(), strError);
        finalizedBudget.SetStrInvalid(strError);
        return false;
    }

    // update expiration
    if (!finalizedBudget.UpdateValid(nCurrentHeight)) {
        LogPrint(BCLog::GMBUDGET,"%s: invalid finalized budget: %s %s\n", __func__, nHash.ToString(), finalizedBudget.IsInvalidLogStr());
        return false;
    }

    // Compare budget payments with existent proposals, don't care on the order, just verify proposals existence.
    std::vector<CBudgetProposal> vBudget = GetBudget();
    std::map<uint256, CBudgetProposal> mapWinningProposals;
    for (const CBudgetProposal& p: vBudget) { mapWinningProposals.emplace(p.GetHash(), p); }
    if (!finalizedBudget.CheckProposals(mapWinningProposals)) {
        finalizedBudget.SetStrInvalid("Invalid proposals");
        LogPrint(BCLog::GMBUDGET,"%s: Budget finalization does not match with winning proposals\n", __func__);
        // just for now (until v6), request proposals and budget sync in case we are missing them
        if (pfrom) {
            CNetMsgMaker maker(pfrom->GetSendVersion());
            // First, request single proposals that we don't have.
            for (const auto& propId : finalizedBudget.GetProposalsHashes()) {
                if (!g_budgetman.HaveProposal(propId)) {
                    g_connman->PushMessage(pfrom, maker.Make(NetMsgType::BUDGETVOTESYNC, propId));
                }
            }

            // Second a full budget sync for missing votes and the budget finalization that we are rejecting here.
            // Note: this will not make any effect on peers with version <= 70923 as they, invalidly, are blocking
            // follow-up budget sync request for the entire node life cycle.
            uint256 n;
            g_connman->PushMessage(pfrom, maker.Make(NetMsgType::BUDGETVOTESYNC, n));
        }
        return false;
    }

    // Add budget finalization.
    SetBudgetProposalsStr(finalizedBudget);
    ForceAddFinalizedBudget(nHash, feeTxId, finalizedBudget);

    LogPrint(BCLog::GMBUDGET,"%s: finalized budget %s [%s (%s)] added\n",
            __func__, nHash.ToString(), finalizedBudget.GetName(), finalizedBudget.GetProposalsStr());
    return true;
}

void CBudgetManager::ForceAddFinalizedBudget(const uint256& nHash, const uint256& feeTxId, const CFinalizedBudget& finalizedBudget)
{
    LOCK(cs_budgets);
    mapFinalizedBudgets.emplace(nHash, finalizedBudget);
    // Add to feeTx index
    mapFeeTxToBudget.emplace(feeTxId, nHash);
    // Remove the budget from the unconfirmed map, if it was there
    if (mapUnconfirmedFeeTx.count(nHash))
        mapUnconfirmedFeeTx.erase(nHash);
}

bool CBudgetManager::AddProposal(CBudgetProposal& budgetProposal)
{
    AssertLockNotHeld(cs_proposals);    // need to lock cs_main here (CheckCollateral)
    const uint256& nHash = budgetProposal.GetHash();

    if (WITH_LOCK(cs_proposals, return mapProposals.count(nHash))) {
        LogPrint(BCLog::GMBUDGET,"%s: proposal %s already added\n", __func__, nHash.ToString());
        return false;
    }

    if (!budgetProposal.IsWellFormed(GetTotalBudget(budgetProposal.GetBlockStart()))) {
        LogPrint(BCLog::GMBUDGET,"%s: Invalid budget proposal %s %s\n", __func__, nHash.ToString(), budgetProposal.IsInvalidLogStr());
        return false;
    }

    std::string strError;
    int nCurrentHeight = GetBestHeight();
    const uint256& feeTxId = budgetProposal.GetFeeTXHash();
    if (!CheckCollateral(feeTxId, nHash, strError, budgetProposal.nTime, nCurrentHeight, false)) {
        LogPrint(BCLog::GMBUDGET,"%s: invalid budget proposal (%s) collateral id=%s - %s\n",
                __func__, nHash.ToString(), feeTxId.ToString(), strError);
        budgetProposal.SetStrInvalid(strError);
        return false;
    }

    // update expiration / heavily-downvoted
    int gmCount = gamemasterman.CountEnabled();
    if (!budgetProposal.UpdateValid(nCurrentHeight, gmCount)) {
        LogPrint(BCLog::GMBUDGET,"%s: Invalid budget proposal %s %s\n", __func__, nHash.ToString(), budgetProposal.IsInvalidLogStr());
        return false;
    }

    {
        LOCK(cs_proposals);
        mapProposals.emplace(nHash, budgetProposal);
        // Add to feeTx index
        mapFeeTxToProposal.emplace(feeTxId, nHash);
    }
    LogPrint(BCLog::GMBUDGET,"%s: budget proposal %s [%s] added\n", __func__, nHash.ToString(), budgetProposal.GetName());

    return true;
}

void CBudgetManager::CheckAndRemove()
{
    int nCurrentHeight = GetBestHeight();
    std::map<uint256, CFinalizedBudget> tmpMapFinalizedBudgets;
    std::map<uint256, CBudgetProposal> tmpMapProposals;

    // Get GM count, used for the heavily down-voted check
    int gmCount = gamemasterman.CountEnabled();

    // Check Proposals first
    {
        LOCK(cs_proposals);
        LogPrint(BCLog::GMBUDGET, "%s: mapProposals cleanup - size before: %d\n", __func__, mapProposals.size());
        for (auto& it: mapProposals) {
            CBudgetProposal* pbudgetProposal = &(it.second);
            if (!pbudgetProposal->UpdateValid(nCurrentHeight, gmCount)) {
                LogPrint(BCLog::GMBUDGET,"%s: Invalid budget proposal %s %s\n", __func__, (it.first).ToString(), pbudgetProposal->IsInvalidLogStr());
                mapFeeTxToProposal.erase(pbudgetProposal->GetFeeTXHash());
            } else {
                 LogPrint(BCLog::GMBUDGET,"%s: Found valid budget proposal: %s %s\n", __func__,
                          pbudgetProposal->GetName(), pbudgetProposal->GetFeeTXHash().ToString());
                 tmpMapProposals.emplace(pbudgetProposal->GetHash(), *pbudgetProposal);
            }
        }
        // Remove invalid entries by overwriting complete map
        mapProposals.swap(tmpMapProposals);
        LogPrint(BCLog::GMBUDGET, "%s: mapProposals cleanup - size after: %d\n", __func__, mapProposals.size());
    }

    // Then check finalized budgets
    {
        LOCK(cs_budgets);
        LogPrint(BCLog::GMBUDGET, "%s: mapFinalizedBudgets cleanup - size before: %d\n", __func__, mapFinalizedBudgets.size());
        for (auto& it: mapFinalizedBudgets) {
            CFinalizedBudget* pfinalizedBudget = &(it.second);
            if (!pfinalizedBudget->UpdateValid(nCurrentHeight)) {
                LogPrint(BCLog::GMBUDGET,"%s: Invalid finalized budget %s %s\n", __func__, (it.first).ToString(), pfinalizedBudget->IsInvalidLogStr());
                mapFeeTxToBudget.erase(pfinalizedBudget->GetFeeTXHash());
            } else {
                LogPrint(BCLog::GMBUDGET,"%s: Found valid finalized budget: %s %s\n", __func__,
                          pfinalizedBudget->GetName(), pfinalizedBudget->GetFeeTXHash().ToString());
                tmpMapFinalizedBudgets.emplace(pfinalizedBudget->GetHash(), *pfinalizedBudget);
            }
        }
        // Remove invalid entries by overwriting complete map
        mapFinalizedBudgets = tmpMapFinalizedBudgets;
        LogPrint(BCLog::GMBUDGET, "%s: mapFinalizedBudgets cleanup - size after: %d\n", __func__, mapFinalizedBudgets.size());
    }
    // Gamemasters vote on valid ones
    VoteOnFinalizedBudgets();
}

void CBudgetManager::RemoveByFeeTxId(const uint256& feeTxId)
{
    {
        LOCK(cs_proposals);
        // Is this collateral related to a proposal?
        const auto& it = mapFeeTxToProposal.find(feeTxId);
        if (it != mapFeeTxToProposal.end()) {
            // Remove proposal
            CBudgetProposal* p = FindProposal(it->second);
            if (p) {
                LogPrintf("%s: Removing proposal %s (collateral disconnected, id=%s)\n", __func__, p->GetName(), feeTxId.ToString());
                {
                    // Erase seen/orhpan votes
                    LOCK(cs_votes);
                    for (const auto& vote: p->GetVotes()) {
                        const uint256& hash{vote.second.GetHash()};
                        mapSeenProposalVotes.erase(hash);
                        mapOrphanProposalVotes.erase(hash);
                    }
                }
                // Erase proposal object
                mapProposals.erase(it->second);
            }
            // Remove from collateral index
            mapFeeTxToProposal.erase(it);
            return;
        }
    }
    {
        LOCK(cs_budgets);
        // Is this collateral related to a finalized budget?
        const auto& it = mapFeeTxToBudget.find(feeTxId);
        if (it != mapFeeTxToBudget.end()) {
            // Remove finalized budget
            CFinalizedBudget* b = FindFinalizedBudget(it->second);
            if (b) {
                LogPrintf("%s: Removing finalized budget %s (collateral disconnected, id=%s)\n", __func__, b->GetName(), feeTxId.ToString());
                {
                    // Erase seen/orhpan votes
                    LOCK(cs_finalizedvotes);
                    for (const uint256& hash: b->GetVotesHashes()) {
                        mapSeenFinalizedBudgetVotes.erase(hash);
                        mapOrphanFinalizedBudgetVotes.erase(hash);
                    }
                }
                // Erase finalized budget object
                mapFinalizedBudgets.erase(it->second);
            }
            // Remove from collateral index
            mapFeeTxToBudget.erase(it);
        }
    }
}

CBudgetManager::HighestFinBudget CBudgetManager::GetBudgetWithHighestVoteCount(int chainHeight) const
{
    LOCK(cs_budgets);
    int highestVoteCount = 0;
    const CFinalizedBudget* pHighestBudget = nullptr;
    for (const auto& it: mapFinalizedBudgets) {
        const CFinalizedBudget* pfinalizedBudget = &(it.second);
        int voteCount = pfinalizedBudget->GetVoteCount();
        if (voteCount > highestVoteCount &&
            chainHeight >= pfinalizedBudget->GetBlockStart() &&
            chainHeight <= pfinalizedBudget->GetBlockEnd()) {
            pHighestBudget = pfinalizedBudget;
            highestVoteCount = voteCount;
        }
    }
    return {pHighestBudget, highestVoteCount};
}

int CBudgetManager::GetHighestVoteCount(int chainHeight) const
{
    const auto& highestBudFin = GetBudgetWithHighestVoteCount(chainHeight);
    return (highestBudFin.m_budget_fin ? highestBudFin.m_vote_count : -1);
}

bool CBudgetManager::GetPayeeAndAmount(int chainHeight, CScript& payeeRet, CAmount& nAmountRet) const
{
    int nCountThreshold;
    if (!IsBudgetPaymentBlock(chainHeight, nCountThreshold))
        return false;

    const auto& highestBudFin = GetBudgetWithHighestVoteCount(chainHeight);
    const CFinalizedBudget* pfb = highestBudFin.m_budget_fin;
    return pfb && pfb->GetPayeeAndAmount(chainHeight, payeeRet, nAmountRet) && highestBudFin.m_vote_count > nCountThreshold;
}

bool CBudgetManager::GetExpectedPayeeAmount(int chainHeight, CAmount& nAmountRet) const
{
    CScript payeeRet;
    return GetPayeeAndAmount(chainHeight, payeeRet, nAmountRet);
}

bool CBudgetManager::FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const int nHeight, bool fProofOfStake) const
{
    if (nHeight <= 0) return false;

    CScript payee;
    CAmount nAmount = 0;

    if (!GetPayeeAndAmount(nHeight, payee, nAmount))
        return false;

    CAmount blockValue = GetBlockValue(nHeight);

    // Starting from hemis v6.0 gamemaster and budgets are paid in the coinbase tx of PoS blocks
    const bool fPayCoinstake = fProofOfStake &&
                               !Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V6_0);

    if (fProofOfStake) {
        if (fPayCoinstake) {
            unsigned int i = txCoinstake.vout.size();
            txCoinstake.vout.resize(i + 1);
            txCoinstake.vout[i].scriptPubKey = payee;
            txCoinstake.vout[i].nValue = nAmount;
        } else {
            txCoinbase.vout.resize(1);
            txCoinbase.vout[0].scriptPubKey = payee;
            txCoinbase.vout[0].nValue = nAmount;
        }
    } else {
        //miners get the full amount on these blocks
        txCoinbase.vout[0].nValue = blockValue;
        txCoinbase.vout.resize(2);

        //these are super blocks, so their value can be much larger than normal
        txCoinbase.vout[1].scriptPubKey = payee;
        txCoinbase.vout[1].nValue = nAmount;
    }

    CTxDestination address;
    ExtractDestination(payee, address);
    LogPrint(BCLog::GMBUDGET,"%s: Budget payment to %s for %lld\n", __func__, EncodeDestination(address), nAmount);
    return true;
}

void CBudgetManager::VoteOnFinalizedBudgets()
{
    // function called only from initialized gamemasters
    if (!fMasterNode) {
        LogPrint(BCLog::GMBUDGET,"%s: Not a gamemaster\n", __func__);
        return;
    }

    // Do this 1 in 4 blocks -- spread out the voting activity
    // -- this function is only called every fourteenth block, so this is really 1 in 56 blocks
    if (GetRandInt(4) != 0) {
        LogPrint(BCLog::GMBUDGET,"%s: waiting\n", __func__);
        return;
    }

    // Get the active gamemaster (operator) key
    CTxIn gmVin;
    Optional<CKey> gmKey{nullopt};
    CBLSSecretKey blsKey;
    if (!GetActiveGamemasterKeys(gmVin, gmKey, blsKey)) {
        return;
    }

    std::vector<CBudgetProposal> vBudget = GetBudget();
    if (vBudget.empty()) {
        LogPrint(BCLog::GMBUDGET,"%s: No proposal can be finalized\n", __func__);
        return;
    }

    std::map<uint256, CBudgetProposal> mapWinningProposals;
    for (const CBudgetProposal& p: vBudget) {
        mapWinningProposals.emplace(p.GetHash(), p);
    }
    // Vector containing the hash of finalized budgets to sign
    std::vector<uint256> vBudgetHashes;
    {
        LOCK(cs_budgets);
        for (auto& it: mapFinalizedBudgets) {
            CFinalizedBudget* pfb = &(it.second);
            // we only need to check this once
            if (pfb->IsAutoChecked()) continue;
            pfb->SetAutoChecked(true);
            //only vote for exact matches
            if (strBudgetMode == "auto") {
                // compare budget payments with winning proposals
                if (!pfb->CheckProposals(mapWinningProposals)) {
                    continue;
                }
            }
            // exact match found. add budget hash to sign it later.
            vBudgetHashes.emplace_back(pfb->GetHash());
        }
    }

    // Sign finalized budgets
    for (const uint256& budgetHash: vBudgetHashes) {
        CFinalizedBudgetVote vote(gmVin, budgetHash);
        if (gmKey != nullopt) {
            // Legacy GM
            if (!vote.Sign(*gmKey, gmKey->GetPubKey().GetID())) {
                LogPrintf("%s: Failure to sign budget %s\n", __func__, budgetHash.ToString());
                continue;
            }
        } else {
            // DGM
            if (!vote.Sign(blsKey)) {
                LogPrintf("%s: Failure to sign budget %s with DGM\n", __func__, budgetHash.ToString());
                continue;
            }
        }
        std::string strError = "";
        if (!UpdateFinalizedBudget(vote, nullptr, strError)) {
            LogPrintf("%s: Error submitting vote - %s\n", __func__, strError);
            continue;
        }
        LogPrint(BCLog::GMBUDGET, "%s: new finalized budget vote signed: %s\n", __func__, vote.GetHash().ToString());
        AddSeenFinalizedBudgetVote(vote);
        vote.Relay();
    }
}

CFinalizedBudget* CBudgetManager::FindFinalizedBudget(const uint256& nHash)
{
    AssertLockHeld(cs_budgets);
    auto it = mapFinalizedBudgets.find(nHash);
    return it != mapFinalizedBudgets.end() ? &(it->second) : nullptr;
}

const CBudgetProposal* CBudgetManager::FindProposalByName(const std::string& strProposalName) const
{
    LOCK(cs_proposals);

    int64_t nYesCountMax = std::numeric_limits<int64_t>::min();
    const CBudgetProposal* pbudgetProposal = nullptr;

    for (const auto& it: mapProposals) {
        const CBudgetProposal& proposal = it.second;
        int64_t nYesCount = proposal.GetYeas() - proposal.GetNays();
        if (proposal.GetName() == strProposalName && nYesCount > nYesCountMax) {
            pbudgetProposal = &proposal;
            nYesCountMax = nYesCount;
        }
    }

    return pbudgetProposal;
}

CBudgetProposal* CBudgetManager::FindProposal(const uint256& nHash)
{
    AssertLockHeld(cs_proposals);
    auto it = mapProposals.find(nHash);
    return it != mapProposals.end() ? &(it->second) : nullptr;
}

bool CBudgetManager::GetProposal(const uint256& nHash, CBudgetProposal& bp) const
{
    LOCK(cs_proposals);
    auto it = mapProposals.find(nHash);
    if (it == mapProposals.end()) return false;
    bp = it->second;
    return true;
}

bool CBudgetManager::GetFinalizedBudget(const uint256& nHash, CFinalizedBudget& fb) const
{
    LOCK(cs_budgets);
    auto it = mapFinalizedBudgets.find(nHash);
    if (it == mapFinalizedBudgets.end()) return false;
    fb = it->second;
    return true;
}

bool CBudgetManager::IsBudgetPaymentBlock(int nBlockHeight, int& nCountThreshold) const
{
    int nHighestCount = GetHighestVoteCount(nBlockHeight);
    int nCountEnabled = gamemasterman.CountEnabled();
    int nFivePercent = nCountEnabled / 20;
    // threshold for highest finalized budgets (highest vote count - 10% of active gamemasters)
    nCountThreshold = nHighestCount - (nCountEnabled / 10);
    // reduce the threshold if there are less than 10 enabled gamemasters
    if (nCountThreshold == nHighestCount) nCountThreshold--;

    LogPrint(BCLog::GMBUDGET,"%s: nHighestCount: %lli, 5%% of Gamemasters: %lli.\n",
            __func__, nHighestCount, nFivePercent);

    // If budget doesn't have 5% of the network votes, then we should pay a gamemaster instead
    return (nHighestCount > nFivePercent);
}

bool CBudgetManager::IsBudgetPaymentBlock(int nBlockHeight) const
{
    int nCountThreshold;
    return IsBudgetPaymentBlock(nBlockHeight, nCountThreshold);
}

TrxValidationStatus CBudgetManager::IsTransactionValid(const CTransaction& txNew, const uint256& nBlockHash, int nBlockHeight) const
{
    int nCountThreshold = 0;
    if (!IsBudgetPaymentBlock(nBlockHeight, nCountThreshold)) {
        // If budget doesn't have 5% of the network votes, then we should pay a gamemaster instead
        return TrxValidationStatus::InValid;
    }

    // check the highest finalized budgets (- 10% to assist in consensus)
    bool fThreshold = false;
    {
        LOCK(cs_budgets);
        // Get the finalized budget with the highest amount of votes..
        const auto& highestBudFin = GetBudgetWithHighestVoteCount(nBlockHeight);
        const CFinalizedBudget* highestVotesBudget = highestBudFin.m_budget_fin;
        if (highestVotesBudget) {
            // Need to surpass the threshold
            if (highestBudFin.m_vote_count > nCountThreshold) {
                fThreshold = true;
                if (highestVotesBudget->IsTransactionValid(txNew, nBlockHash, nBlockHeight) ==
                    TrxValidationStatus::Valid) {
                    return TrxValidationStatus::Valid;
                }
            }
            // tx not valid
            LogPrint(BCLog::GMBUDGET, "%s: ignoring budget. Out of range or tx not valid.\n", __func__);
        }
    }

    // If not enough gamemasters autovoted for any of the finalized budgets or if none of the txs
    // are valid, we should pay a gamemaster instead
    return fThreshold ? TrxValidationStatus::InValid : TrxValidationStatus::VoteThreshold;
}

std::vector<CBudgetProposal*> CBudgetManager::GetAllProposalsOrdered()
{
    LOCK(cs_proposals);
    std::vector<CBudgetProposal*> vBudgetProposalRet;
    for (auto& it: mapProposals) {
        CBudgetProposal* pbudgetProposal = &(it.second);
        RemoveStaleVotesOnProposal(pbudgetProposal);
        vBudgetProposalRet.push_back(pbudgetProposal);
    }
    std::sort(vBudgetProposalRet.begin(), vBudgetProposalRet.end(), CBudgetProposal::PtrHigherYes);
    return vBudgetProposalRet;
}

std::vector<CBudgetProposal> CBudgetManager::GetBudget()
{
    LOCK(cs_proposals);

    int nHeight = GetBestHeight();
    if (nHeight <= 0)
        return {};

    // ------- Get proposals ordered by votes (highest to lowest)
    std::vector<CBudgetProposal*> vProposalsOrdered = GetAllProposalsOrdered();

    // ------- Grab The Budgets In Order
    std::vector<CBudgetProposal> vBudgetProposalsRet;
    CAmount nBudgetAllocated = 0;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = nHeight - nHeight % nBlocksPerCycle + nBlocksPerCycle;
    int nBlockEnd = nBlockStart + nBlocksPerCycle - 1;
    int gmCount = gamemasterman.CountEnabled();
    CAmount nTotalBudget = GetTotalBudget(nBlockStart);

    for (CBudgetProposal* pbudgetProposal: vProposalsOrdered) {
        LogPrint(BCLog::GMBUDGET,"%s: Processing Budget %s\n", __func__, pbudgetProposal->GetName());
        //prop start/end should be inside this period
        if (pbudgetProposal->IsPassing(nBlockStart, nBlockEnd, gmCount)) {
            LogPrint(BCLog::GMBUDGET,"%s:  -   Check 1 passed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                    __func__, pbudgetProposal->IsValid(), pbudgetProposal->GetBlockStart(), nBlockStart, pbudgetProposal->GetBlockEnd(),
                    nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), gmCount / 10, pbudgetProposal->IsEstablished());

            if (pbudgetProposal->GetAmount() + nBudgetAllocated <= nTotalBudget) {
                pbudgetProposal->SetAllotted(pbudgetProposal->GetAmount());
                nBudgetAllocated += pbudgetProposal->GetAmount();
                vBudgetProposalsRet.emplace_back(*pbudgetProposal);
                LogPrint(BCLog::GMBUDGET,"%s:  -     Check 2 passed: Budget added\n", __func__);
            } else {
                pbudgetProposal->SetAllotted(0);
                LogPrint(BCLog::GMBUDGET,"%s:  -     Check 2 failed: no amount allotted\n", __func__);
            }

        } else {
            LogPrint(BCLog::GMBUDGET,"%s:  -   Check 1 failed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                    __func__, pbudgetProposal->IsValid(), pbudgetProposal->GetBlockStart(), nBlockStart, pbudgetProposal->GetBlockEnd(),
                    nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), gamemasterman.CountEnabled() / 10,
                    pbudgetProposal->IsEstablished());
        }

    }

    return vBudgetProposalsRet;
}

std::vector<CFinalizedBudget*> CBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs_budgets);

    std::vector<CFinalizedBudget*> vFinalizedBudgetsRet;

    // ------- Grab The Budgets In Order
    for (auto& it: mapFinalizedBudgets) {
        vFinalizedBudgetsRet.push_back(&(it.second));
    }
    std::sort(vFinalizedBudgetsRet.begin(), vFinalizedBudgetsRet.end(), CFinalizedBudget::PtrGreater);

    return vFinalizedBudgetsRet;
}

std::string CBudgetManager::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_budgets);

    std::string ret = "unknown-budget";

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (nBlockHeight >= pfinalizedBudget->GetBlockStart() && nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            CTxBudgetPayment payment;
            if (pfinalizedBudget->GetBudgetPaymentByBlock(nBlockHeight, payment)) {
                if (ret == "unknown-budget") {
                    ret = payment.nProposalHash.ToString();
                } else {
                    ret += ",";
                    ret += payment.nProposalHash.ToString();
                }
            } else {
                LogPrint(BCLog::GMBUDGET,"%s:  Couldn't find budget payment for block %d\n", __func__, nBlockHeight);
            }
        }

        ++it;
    }

    return ret;
}

CAmount CBudgetManager::GetTotalBudget(int nHeight)
{
    // 100% of block reward after V5.5 upgrade
    CAmount nSubsidy = GetBlockValue(nHeight);

    // 20% of block reward prior to V5.5 upgrade
    if (nHeight <= Params().GetConsensus().vUpgrades[Consensus::UPGRADE_V5_5].nActivationHeight) {
        nSubsidy /= 5;
    }

    // multiplied by the number of blocks in a cycle (144 on testnet, 30*1440 on mainnet)
    return nSubsidy * Params().GetConsensus().nBudgetCycleBlocks;
}

void CBudgetManager::AddSeenProposalVote(const CBudgetVote& vote)
{
    LOCK(cs_votes);
    mapSeenProposalVotes.emplace(vote.GetHash(), vote);
}

void CBudgetManager::AddSeenFinalizedBudgetVote(const CFinalizedBudgetVote& vote)
{
    LOCK(cs_finalizedvotes);
    mapSeenFinalizedBudgetVotes.emplace(vote.GetHash(), vote);
}

void CBudgetManager::RemoveStaleVotesOnProposal(CBudgetProposal* prop)
{
    AssertLockHeld(cs_proposals);
    LogPrint(BCLog::GMBUDGET, "Cleaning proposal votes for %s. Before: YES=%d, NO=%d\n",
            prop->GetName(), prop->GetYeas(), prop->GetNays());

    auto it = prop->mapVotes.begin();
    while (it != prop->mapVotes.end()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        auto dgm = gmList.GetGMByCollateral(it->first);
        if (dgm) {
            (*it).second.SetValid(!dgm->IsPoSeBanned());
        } else {
            // -- Legacy System (!TODO: remove after enforcement) --
            CGamemaster* pgm = gamemasterman.Find(it->first);
            (*it).second.SetValid(pgm && pgm->IsEnabled());
        }
        ++it;
    }

    LogPrint(BCLog::GMBUDGET, "Cleaned proposal votes for %s. After: YES=%d, NO=%d\n",
            prop->GetName(), prop->GetYeas(), prop->GetNays());
}

void CBudgetManager::RemoveStaleVotesOnFinalBudget(CFinalizedBudget* fbud)
{
    AssertLockHeld(cs_budgets);
    LogPrint(BCLog::GMBUDGET, "Cleaning finalized budget votes for [%s (%s)]. Before: %d\n",
            fbud->GetName(), fbud->GetProposalsStr(), fbud->GetVoteCount());

    auto it = fbud->mapVotes.begin();
    while (it != fbud->mapVotes.end()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        auto dgm = gmList.GetGMByCollateral(it->first);
        if (dgm) {
            (*it).second.SetValid(!dgm->IsPoSeBanned());
        } else {
            // -- Legacy System (!TODO: remove after enforcement) --
            CGamemaster* pgm = gamemasterman.Find(it->first);
            (*it).second.SetValid(pgm && pgm->IsEnabled());
        }
        ++it;
    }
    LogPrint(BCLog::GMBUDGET, "Cleaned finalized budget votes for [%s (%s)]. After: %d\n",
            fbud->GetName(), fbud->GetProposalsStr(), fbud->GetVoteCount());
}

CDataStream CBudgetManager::GetProposalVoteSerialized(const uint256& voteHash) const
{
    LOCK(cs_votes);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenProposalVotes.at(voteHash);
    return ss;
}

CDataStream CBudgetManager::GetProposalSerialized(const uint256& propHash) const
{
    LOCK(cs_proposals);
    return mapProposals.at(propHash).GetBroadcast();
}

CDataStream CBudgetManager::GetFinalizedBudgetVoteSerialized(const uint256& voteHash) const
{
    LOCK(cs_finalizedvotes);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(1000);
    ss << mapSeenFinalizedBudgetVotes.at(voteHash);
    return ss;
}

CDataStream CBudgetManager::GetFinalizedBudgetSerialized(const uint256& budgetHash) const
{
    LOCK(cs_budgets);
    return mapFinalizedBudgets.at(budgetHash).GetBroadcast();
}

bool CBudgetManager::AddAndRelayProposalVote(const CBudgetVote& vote, std::string& strError)
{
    if (UpdateProposal(vote, nullptr, strError)) {
        AddSeenProposalVote(vote);
        vote.Relay();
        return true;
    }
    return false;
}

void CBudgetManager::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    if (g_tiertwo_sync_state.GetSyncPhase() <= GAMEMASTER_SYNC_BUDGET) return;

    if (strBudgetMode == "suggest") { //suggest the budget we see
        SubmitFinalBudget();
    }

    int nCurrentHeight = GetBestHeight();
    //this function should be called 1/14 blocks, allowing up to 100 votes per day on all proposals
    if (nCurrentHeight % 14 != 0) return;

    // incremental sync with our peers
    if (g_tiertwo_sync_state.IsSynced()) {
        LogPrint(BCLog::GMBUDGET,"%s:  incremental sync started\n", __func__);
        // Once every 7 days, try to relay the complete budget data
        if (GetRandInt(Params().IsRegTestNet() ? 2 : 720) == 0) {
            ResetSync();
        }

        CBudgetManager* manager = this;
        g_connman->ForEachNode([manager](CNode* pnode){
            if (pnode->nVersion >= ActiveProtocol())
                manager->Sync(pnode, true);
        });
        MarkSynced();
    }

    // remove expired/heavily downvoted budgets
    CheckAndRemove();

    {
        LOCK(cs_proposals);
        LogPrint(BCLog::GMBUDGET,"%s:  mapProposals cleanup - size: %d\n", __func__, mapProposals.size());
        for (auto& it: mapProposals) {
            RemoveStaleVotesOnProposal(&it.second);
        }
    }
    {
        LOCK(cs_budgets);
        LogPrint(BCLog::GMBUDGET,"%s:  mapFinalizedBudgets cleanup - size: %d\n", __func__, mapFinalizedBudgets.size());
        for (auto& it: mapFinalizedBudgets) {
            RemoveStaleVotesOnFinalBudget(&it.second);
        }
    }

    int64_t now = GetTime();
    const auto cleanOrphans = [now](auto& mutex, auto& mapOrphans, auto& mapSeen) {
        LOCK(mutex);
        for (auto it = mapOrphans.begin() ; it != mapOrphans.end();) {
            int64_t lastReceivedVoteTime = it->second.second;
            if (lastReceivedVoteTime + BUDGET_ORPHAN_VOTES_CLEANUP_SECONDS < now) {
                // Clean seen votes
                for (const auto& voteIt : it->second.first) {
                    mapSeen.erase(voteIt.GetHash());
                }
                // Remove proposal orphan votes
                it = mapOrphans.erase(it);
            } else {
                it++;
            }
        }
    };

    // Clean orphan proposal votes if no parent arrived after an hour.
    cleanOrphans(cs_votes, mapOrphanProposalVotes, mapSeenProposalVotes);
    // Clean orphan budget votes if no parent arrived after an hour.
    cleanOrphans(cs_finalizedvotes, mapOrphanFinalizedBudgetVotes, mapSeenFinalizedBudgetVotes);

    // Once every 2 weeks (1/14 * 1/1440), clean the seen maps
    if (g_tiertwo_sync_state.IsSynced() && GetRandInt(1440) == 0) {
        ReloadMapSeen();
    }

    LogPrint(BCLog::GMBUDGET,"%s:  PASSED\n", __func__);
}

int CBudgetManager::ProcessBudgetVoteSync(const uint256& nProp, CNode* pfrom)
{
    if (nProp.IsNull()) {
        LOCK2(cs_budgets, cs_proposals);
        if (!(pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal())) {
            if (g_netfulfilledman.HasFulfilledRequest(pfrom->addr, BUDGET_SYNC_REQUEST_RECV)) {
                LogPrint(BCLog::GAMEMASTER, "budgetsync - peer %i already asked for budget sync\n", pfrom->GetId());
                // let's not be so hard with the node for now.
                return 10;
            }
        }
    }

    if (nProp.IsNull()) Sync(pfrom, false /* fPartial */);
    else SyncSingleItem(pfrom, nProp);
    LogPrint(BCLog::GMBUDGET, "gmvs - Sent Gamemaster votes to peer %i\n", pfrom->GetId());
    return 0;
}

int CBudgetManager::ProcessProposal(CBudgetProposal& proposal)
{
    const uint256& nHash = proposal.GetHash();
    if (HaveProposal(nHash)) {
        g_tiertwo_sync_state.AddedBudgetItem(nHash);
        return 0;
    }
    if (!AddProposal(proposal)) {
        return 0;
    }

    // Relay only if we are synchronized
    // Makes no sense to relay proposals to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) proposal.Relay();
    g_tiertwo_sync_state.AddedBudgetItem(nHash);

    LogPrint(BCLog::GMBUDGET, "mprop (new) %s\n", nHash.ToString());
    //We might have active votes for this proposal that are valid now
    CheckOrphanVotes();
    return 0;
}

bool CBudgetManager::ProcessProposalVote(CBudgetVote& vote, CNode* pfrom, CValidationState& state)
{
    const uint256& voteID = vote.GetHash();

    if (HaveSeenProposalVote(voteID)) {
        g_tiertwo_sync_state.AddedBudgetItem(voteID);
        return false;
    }

    std::string err;
    if (vote.GetTime() > GetTime() + (60 * 60)) {
        err = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n",
                             vote.GetHash().ToString(), vote.GetTime(), GetTime() + (60 * 60));
        return state.Invalid(false, REJECT_INVALID, "bad-mvote", err);
    }

    const CTxIn& voteVin = vote.GetVin();

    // See if this vote was signed with a deterministic gamemaster
    auto gmList = deterministicGMManager->GetListAtChainTip();
    auto dgm = gmList.GetGMByCollateral(voteVin.prevout);
    if (dgm) {
        const std::string& gm_protx_id = dgm->proTxHash.ToString();

        if (dgm->IsPoSeBanned()) {
            err = strprintf("gamemaster (%s) not valid or PoSe banned", gm_protx_id);
            return state.DoS(0, false, REJECT_INVALID, "bad-mvote", false, err);
        }

        AddSeenProposalVote(vote);

        if (!vote.CheckSignature(dgm->pdgmState->keyIDVoting)) {
            err = strprintf("invalid mvote sig from dgm: %s", gm_protx_id);
            return state.DoS(100, false, REJECT_INVALID, "bad-mvote-sig", false, err);
        }

        if (!UpdateProposal(vote, pfrom, err)) {
            return state.DoS(0, false, REJECT_INVALID, "bad-mvote", false, strprintf("%s (%s)", err, gm_protx_id));
        }

        // Relay only if we are synchronized
        // Makes no sense to relay votes to the peers from where we are syncing them.
        if (g_tiertwo_sync_state.IsSynced()) vote.Relay();
        g_tiertwo_sync_state.AddedBudgetItem(voteID);
        LogPrint(BCLog::GMBUDGET, "mvote - new vote (%s) for proposal %s from dgm %s\n",
                voteID.ToString(), vote.GetProposalHash().ToString(), gm_protx_id);
        return true;
    }

    // -- Legacy System (!TODO: remove after enforcement) --

    CGamemaster* pgm = gamemasterman.Find(voteVin.prevout);
    if (!pgm) {
        err = strprintf("unknown gamemaster - vin: %s", voteVin.prevout.ToString());
        // Ask for GM only if we finished syncing the GM list.
        if (pfrom && g_tiertwo_sync_state.IsGamemasterListSynced()) gamemasterman.AskForGM(pfrom, voteVin);
        return state.DoS(0, false, REJECT_INVALID, "bad-mvote", false, err);
    }

    if (!pgm->IsEnabled()) {
        return state.DoS(0, false, REJECT_INVALID, "bad-mvote", false, "gamemaster not valid");
    }

    AddSeenProposalVote(vote);

    if (!vote.CheckSignature(pgm->pubKeyGamemaster.GetID())) {
        if (g_tiertwo_sync_state.IsSynced()) {
            err = strprintf("signature from gamemaster %s invalid", voteVin.prevout.ToString());
            return state.DoS(20, false, REJECT_INVALID, "bad-mvote-sig", false, err);
        }
        return false;
    }

    if (!UpdateProposal(vote, pfrom, err)) {
        return state.DoS(0, false, REJECT_INVALID, "bad-mvote", false, err);
    }

    // Relay only if we are synchronized
    // Makes no sense to relay votes to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) vote.Relay();
    g_tiertwo_sync_state.AddedBudgetItem(voteID);
    LogPrint(BCLog::GMBUDGET, "mvote - new vote (%s) for proposal %s from dgm %s\n",
            voteID.ToString(), vote.GetProposalHash().ToString(), voteVin.prevout.ToString());
    return true;
}

int CBudgetManager::ProcessFinalizedBudget(CFinalizedBudget& finalbudget, CNode* pfrom)
{

    const uint256& nHash = finalbudget.GetHash();
    if (HaveFinalizedBudget(nHash)) {
        g_tiertwo_sync_state.AddedBudgetItem(nHash);
        return 0;
    }
    if (!AddFinalizedBudget(finalbudget, pfrom)) {
        return 0;
    }

    // Relay only if we are synchronized
    // Makes no sense to relay finalizations to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) finalbudget.Relay();
    g_tiertwo_sync_state.AddedBudgetItem(nHash);

    LogPrint(BCLog::GMBUDGET, "fbs (new) %s\n", nHash.ToString());
    //we might have active votes for this budget that are now valid
    CheckOrphanVotes();
    return 0;
}

bool CBudgetManager::ProcessFinalizedBudgetVote(CFinalizedBudgetVote& vote, CNode* pfrom, CValidationState& state)
{
    const uint256& voteID = vote.GetHash();

    if (HaveSeenFinalizedBudgetVote(voteID)) {
        g_tiertwo_sync_state.AddedBudgetItem(voteID);
        return false;
    }

    std::string err;
    if (vote.GetTime() > GetTime() + (60 * 60)) {
        err = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n",
                             vote.GetHash().ToString(), vote.GetTime(), GetTime() + (60 * 60));
        return state.Invalid(false, REJECT_INVALID, "bad-fbvote", err);
    }

    const CTxIn& voteVin = vote.GetVin();

    // See if this vote was signed with a deterministic gamemaster
    auto gmList = deterministicGMManager->GetListAtChainTip();
    auto dgm = gmList.GetGMByCollateral(voteVin.prevout);
    if (dgm) {
        const std::string& gm_protx_id = dgm->proTxHash.ToString();

        if (dgm->IsPoSeBanned()) {
            err = strprintf("gamemaster (%s) not valid or PoSe banned", gm_protx_id);
            return state.DoS(0, false, REJECT_INVALID, "bad-fbvote", false, err);
        }

        AddSeenFinalizedBudgetVote(vote);

        if (!vote.CheckSignature(dgm->pdgmState->pubKeyOperator.Get())) {
            err = strprintf("invalid fbvote sig from dgm: %s", gm_protx_id);
            return state.DoS(100, false, REJECT_INVALID, "bad-fbvote-sig", false, err);
        }

        if (!UpdateFinalizedBudget(vote, pfrom, err)) {
            return state.DoS(0, false, REJECT_INVALID, "bad-fbvote", false, strprintf("%s (%s)", err, gm_protx_id));
        }

        // Relay only if we are synchronized
        // Makes no sense to relay votes to the peers from where we are syncing them.
        if (g_tiertwo_sync_state.IsSynced()) vote.Relay();
        g_tiertwo_sync_state.AddedBudgetItem(voteID);
        LogPrint(BCLog::GMBUDGET, "fbvote - new vote (%s) for budget %s from dgm %s\n",
                voteID.ToString(), vote.GetBudgetHash().ToString(), gm_protx_id);
        return true;
    }

    // -- Legacy System (!TODO: remove after enforcement) --
    CGamemaster* pgm = gamemasterman.Find(voteVin.prevout);
    if (!pgm) {
        err = strprintf("unknown gamemaster - vin: %s", voteVin.prevout.ToString());
        // Ask for GM only if we finished syncing the GM list.
        if (pfrom && g_tiertwo_sync_state.IsGamemasterListSynced()) gamemasterman.AskForGM(pfrom, voteVin);
        return state.DoS(0, false, REJECT_INVALID, "bad-fbvote", false, err);
    }

    if (!pgm->IsEnabled()) {
        return state.DoS(0, false, REJECT_INVALID, "bad-fbvote", false, "gamemaster not valid");
    }

    AddSeenFinalizedBudgetVote(vote);

    if (!vote.CheckSignature(pgm->pubKeyGamemaster.GetID())) {
        if (g_tiertwo_sync_state.IsSynced()) {
            err = strprintf("signature from gamemaster %s invalid", voteVin.prevout.ToString());
            return state.DoS(20, false, REJECT_INVALID, "bad-fbvote-sig", false, err);
        }
        return false;
    }

    if (!UpdateFinalizedBudget(vote, pfrom, err)) {
        return state.DoS(0, false, REJECT_INVALID, "bad-fbvote", false, err);
    }

    // Relay only if we are synchronized
    // Makes no sense to relay votes to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) vote.Relay();
    g_tiertwo_sync_state.AddedBudgetItem(voteID);
    LogPrint(BCLog::GMBUDGET, "fbvote - new vote (%s) for budget %s from gm %s\n",
            voteID.ToString(), vote.GetBudgetHash().ToString(), voteVin.prevout.ToString());
    return true;
}

bool CBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, int& banScore)
{
    banScore = ProcessMessageInner(pfrom, strCommand, vRecv);
    return banScore == 0;
}

int CBudgetManager::ProcessMessageInner(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) return 0;

    if (strCommand == NetMsgType::BUDGETVOTESYNC) {
        // Gamemaster vote sync
        uint256 nProp;
        vRecv >> nProp;
        return ProcessBudgetVoteSync(nProp, pfrom);
    }

    if (strCommand == NetMsgType::BUDGETPROPOSAL) {
        // Gamemaster Proposal
        CBudgetProposal proposal;
        if (!proposal.ParseBroadcast(vRecv)) {
            return 20;
        }
        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(proposal.GetHash(), MSG_BUDGET_PROPOSAL);
        }
        return ProcessProposal(proposal);
    }

    if (strCommand == NetMsgType::BUDGETVOTE) {
        CBudgetVote vote;
        vRecv >> vote;
        vote.SetValid(true);

        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(vote.GetHash(), MSG_BUDGET_VOTE);
        }

        CValidationState state;
        if (!ProcessProposalVote(vote, pfrom, state)) {
            int nDos = 0;
            if (state.IsInvalid(nDos)) {
                LogPrint(BCLog::GMBUDGET, "%s: %s\n", __func__, FormatStateMessage(state));
            }
            return nDos;
        }
        return 0;
    }

    if (strCommand == NetMsgType::FINALBUDGET) {
        // Finalized Budget Suggestion
        CFinalizedBudget finalbudget;
        if (!finalbudget.ParseBroadcast(vRecv)) {
            return 20;
        }
        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(finalbudget.GetHash(), MSG_BUDGET_FINALIZED);
        }
        return ProcessFinalizedBudget(finalbudget, pfrom);
    }

    if (strCommand == NetMsgType::FINALBUDGETVOTE) {
        CFinalizedBudgetVote vote;
        vRecv >> vote;
        vote.SetValid(true);

        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(vote.GetHash(), MSG_BUDGET_FINALIZED_VOTE);
        }

        CValidationState state;
        if (!ProcessFinalizedBudgetVote(vote, pfrom, state)) {
            int nDos = 0;
            if (state.IsInvalid(nDos)) {
                LogPrint(BCLog::GMBUDGET, "%s: %s\n", __func__, FormatStateMessage(state));
            }
            return nDos;
        }
        return 0;
    }

    // nothing was done
    return 0;
}

void CBudgetManager::SetSynced(bool synced)
{
    {
        LOCK(cs_proposals);
        for (auto& it: mapProposals) {
            CBudgetProposal* pbudgetProposal = &(it.second);
            if (pbudgetProposal && pbudgetProposal->IsValid()) {
                //mark votes
                pbudgetProposal->SetSynced(synced);
            }
        }
    }
    {
        LOCK(cs_budgets);
        for (auto& it: mapFinalizedBudgets) {
            CFinalizedBudget* pfinalizedBudget = &(it.second);
            if (pfinalizedBudget && pfinalizedBudget->IsValid()) {
                //mark votes
                pfinalizedBudget->SetSynced(synced);
            }
        }
    }
}

template<typename T>
static bool relayItemIfFound(const uint256& itemHash, CNode* pfrom, RecursiveMutex& cs, std::map<uint256, T>& map, const char* type)
{
    CNetMsgMaker msgMaker(pfrom->GetSendVersion());
    LOCK(cs);
    const auto& it = map.find(itemHash);
    if (it == map.end()) return false;
    T* item = &(it->second);
    if (!item->IsValid()) return true; // don't broadcast invalid items
    g_connman->PushMessage(pfrom, msgMaker.Make(type, item->GetBroadcast()));
    int nInvCount = 1;
    item->SyncVotes(pfrom, false /* fPartial */, nInvCount);
    LogPrint(BCLog::GMBUDGET, "%s: single %s sent %d items\n", __func__, type, nInvCount);
    return true;
}

template<typename T>
static void relayInventoryItems(CNode* pfrom, RecursiveMutex& cs, std::map<uint256, T>& map, bool fPartial, GetDataMsg invType, const int gm_sync_budget_type)
{
    CNetMsgMaker msgMaker(pfrom->GetSendVersion());
    int nInvCount = 0;
    {
        LOCK(cs);
        for (auto& it: map) {
            T* item = &(it.second);
            if (item && item->IsValid()) {
                pfrom->PushInventory(CInv(invType, item->GetHash()));
                nInvCount++;
                item->SyncVotes(pfrom, fPartial, nInvCount);
            }
        }
    }
    g_connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, gm_sync_budget_type, nInvCount));
    LogPrint(BCLog::GMBUDGET, "%s: sent %d items\n", __func__, nInvCount);
}

void CBudgetManager::SyncSingleItem(CNode* pfrom, const uint256& nProp)
{
    if (nProp.IsNull()) return;
    // Try first to relay a proposal
    if (relayItemIfFound<CBudgetProposal>(nProp, pfrom, cs_proposals, mapProposals, NetMsgType::BUDGETPROPOSAL)) {
        return;
    }
    // Try now to relay a finalization
    if (relayItemIfFound<CFinalizedBudget>(nProp, pfrom, cs_budgets, mapFinalizedBudgets, NetMsgType::FINALBUDGET)) {
        return;
    }
    LogPrint(BCLog::GMBUDGET, "%s: single request budget item not found\n", __func__);
}


void CBudgetManager::Sync(CNode* pfrom, bool fPartial)
{
    // Full budget sync request.
    relayInventoryItems<CBudgetProposal>(pfrom, cs_proposals, mapProposals, fPartial, MSG_BUDGET_PROPOSAL, GAMEMASTER_SYNC_BUDGET_PROP);
    relayInventoryItems<CFinalizedBudget>(pfrom, cs_budgets, mapFinalizedBudgets, fPartial, MSG_BUDGET_FINALIZED, GAMEMASTER_SYNC_BUDGET_FIN);

    if (!fPartial) {
        // We are not going to answer full budget sync requests for an hour (chainparams.FulfilledRequestExpireTime()).
        // The remote peer can still do single prop and gmv sync requests if needed.
        g_netfulfilledman.AddFulfilledRequest(pfrom->addr, BUDGET_SYNC_REQUEST_RECV);
    }
}

template<typename T>
static void TryAppendOrphanVoteMap(const T& vote,
                                   const uint256& parentHash,
                                   std::map<uint256, std::pair<std::vector<T>, int64_t>>& mapOrphan,
                                   std::map<uint256, T>& mapSeen)
{
    if (mapOrphan.size() > ORPHAN_VOTES_CACHE_LIMIT) {
        // future: notify user about this
        mapSeen.erase(vote.GetHash());
    } else {
        // Append orphan vote
        const auto& it = mapOrphan.find(parentHash);
        if (it != mapOrphan.end()) {
            // Check size limit and erase it from the seen map if we already passed it
            if (it->second.first.size() > ORPHAN_VOTES_CACHE_LIMIT) {
                // future: check if the GM already voted and replace vote
                mapSeen.erase(vote.GetHash());
            } else {
                it->second.first.emplace_back(vote);
                it->second.second = GetTime();
            }
        } else {
            mapOrphan.emplace(parentHash, std::make_pair<std::vector<T>, int64_t>({vote}, GetTime()));
        }
    }
}

bool CBudgetManager::UpdateProposal(const CBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs_proposals);

    const uint256& nProposalHash = vote.GetProposalHash();
    const auto& itProposal = mapProposals.find(nProposalHash);
    if (itProposal == mapProposals.end()) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!g_tiertwo_sync_state.IsSynced()) return false;

            LogPrint(BCLog::GMBUDGET,"%s: Unknown proposal %d, asking for source proposal\n", __func__, nProposalHash.ToString());
            {
                LOCK(cs_votes);
                TryAppendOrphanVoteMap<CBudgetVote>(vote, nProposalHash, mapOrphanProposalVotes, mapSeenProposalVotes);
            }

            if (!g_netfulfilledman.HasItemRequest(pfrom->addr, nProposalHash)) {
                g_connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::BUDGETVOTESYNC, nProposalHash));
                g_netfulfilledman.AddItemRequest(pfrom->addr, nProposalHash);
            }
        }

        strError = "Proposal not found!";
        return false;
    }

    // Add or update vote
    return itProposal->second.AddOrUpdateVote(vote, strError);
}

bool CBudgetManager::UpdateFinalizedBudget(const CFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs_budgets);

    const uint256& nBudgetHash = vote.GetBudgetHash();
    if (!mapFinalizedBudgets.count(nBudgetHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!g_tiertwo_sync_state.IsSynced()) return false;

            LogPrint(BCLog::GMBUDGET,"%s: Unknown Finalized Proposal %s, asking for source budget\n", __func__, nBudgetHash.ToString());
            {
                LOCK(cs_finalizedvotes);
                TryAppendOrphanVoteMap<CFinalizedBudgetVote>(vote, nBudgetHash, mapOrphanFinalizedBudgetVotes, mapSeenFinalizedBudgetVotes);
            }

            if (!g_netfulfilledman.HasItemRequest(pfrom->addr, nBudgetHash)) {
                g_connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::BUDGETVOTESYNC, nBudgetHash));
                g_netfulfilledman.AddItemRequest(pfrom->addr, nBudgetHash);
            }
        }

        strError = "Finalized Budget " + nBudgetHash.ToString() +  " not found!";
        return false;
    }
    LogPrint(BCLog::GMBUDGET,"%s: Finalized Proposal %s added\n", __func__, nBudgetHash.ToString());
    return mapFinalizedBudgets[nBudgetHash].AddOrUpdateVote(vote, strError);
}

std::string CBudgetManager::ToString() const
{
    unsigned int nProposals = WITH_LOCK(cs_proposals, return mapProposals.size(); );
    unsigned int nBudgets = WITH_LOCK(cs_budgets, return mapFinalizedBudgets.size(); );

    unsigned int nSeenVotes = 0, nOrphanVotes = 0;
    {
        LOCK(cs_votes);
        nSeenVotes = mapSeenProposalVotes.size();
        nOrphanVotes = mapOrphanProposalVotes.size();
    }

    unsigned int nSeenFinalizedVotes = 0, nOrphanFinalizedVotes = 0;
    {
        LOCK(cs_finalizedvotes);
        nSeenFinalizedVotes = mapSeenFinalizedBudgetVotes.size();
        nOrphanFinalizedVotes = mapOrphanFinalizedBudgetVotes.size();
    }

    return strprintf("Proposals: %d - Finalized Budgets: %d - "
            "Proposal Votes: %d (orphan: %d) - "
            "Finalized Budget Votes: %d (orphan: %d)",
            nProposals, nBudgets,
            nSeenVotes, nOrphanVotes, nSeenFinalizedVotes, nOrphanFinalizedVotes);
}


/*
 * Check Collateral
 */
bool CheckCollateralConfs(const uint256& nTxCollateralHash, int nCurrentHeight, int nProposalHeight, std::string& strError)
{
    const int nRequiredConfs = Params().GetConsensus().nBudgetFeeConfirmations;
    const int nConf = nCurrentHeight - nProposalHeight + 1;

    if (nConf < nRequiredConfs) {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations (current height: %d, fee tx height: %d)",
                nRequiredConfs, nConf, nCurrentHeight, nProposalHeight);
        LogPrint(BCLog::GMBUDGET,"%s: %s\n", __func__, strError);
        return false;
    }
    return true;
}

bool CheckCollateral(const uint256& nTxCollateralHash, const uint256& nExpectedHash, std::string& strError, int64_t& nTime, int nCurrentHeight, bool fBudgetFinalization)
{
    CTransactionRef txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", nTxCollateralHash.ToString());
        return false;
    }

    if (txCollateral->vout.size() < 1) return false;
    if (txCollateral->nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    for (const CTxOut &o : txCollateral->vout) {
        if (!o.scriptPubKey.IsPayToPublicKeyHash() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral->ToString());
            return false;
        }
        if (fBudgetFinalization) {
            // Collateral for budget finalization
            // Note: there are still old valid budgets out there, but the check for the new 5 HMS finalization collateral
            //       will also cover the old 50 HMS finalization collateral.
            LogPrint(BCLog::GMBUDGET, "Final Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::GMBUDGET, "Final Budget: o.nValue(%ld) >= BUDGET_FEE_TX(%ld) ?\n", o.nValue, BUDGET_FEE_TX);
                if(o.nValue >= BUDGET_FEE_TX) {
                    foundOpReturn = true;
                    break;
                }
            }
        } else {
            // Collateral for normal budget proposal
            LogPrint(BCLog::GMBUDGET, "Normal Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::GMBUDGET, "Normal Budget: o.nValue(%ld) >= PROPOSAL_FEE_TX(%ld) ?\n", o.nValue, PROPOSAL_FEE_TX);
                if(o.nValue >= PROPOSAL_FEE_TX) {
                    foundOpReturn = true;
                    break;
                }
            }
        }
    }

    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral->ToString());
        return false;
    }

    // Retrieve block height (checking that it's in the active chain) and time
    // both get set in CBudgetProposal/CFinalizedBudget by the caller (AddProposal/AddFinalizedBudget)
    if (nBlockHash.IsNull()) {
        strError = strprintf("Collateral transaction %s is unconfirmed", nTxCollateralHash.ToString());
        return false;
    }
    nTime = 0;
    int nProposalHeight = 0;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = LookupBlockIndex(nBlockHash);
        if (pindex && chainActive.Contains(pindex)) {
            nProposalHeight = pindex->nHeight;
            nTime = pindex->nTime;
        }
    }

    if (!nProposalHeight) {
        strError = strprintf("Collateral transaction %s not in Active chain", nTxCollateralHash.ToString());
        return false;
    }

    return CheckCollateralConfs(nTxCollateralHash, nCurrentHeight, nProposalHeight, strError);
}
