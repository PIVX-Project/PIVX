// Copyright (c) 2014-2015 The Dash developers
//Copyright (c) 2015-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "chainparams.h"
#include "fs.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternode.h"
#include "masternodeman.h"
#include "util.h"


CBudgetManager budget;
RecursiveMutex cs_budget;

std::map<uint256, int64_t> askedForSourceProposalOrBudget;
std::vector<CBudgetProposalBroadcast> vecImmatureBudgetProposals;
std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;

int nSubmittedFinalBudget;

bool IsBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf, bool fBudgetFinalization)
{
    CTransaction txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", txCollateral.ToString());
        LogPrint(BCLog::MNBUDGET,"CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    for (const CTxOut &o : txCollateral.vout) {
        if (!o.scriptPubKey.IsNormalPaymentScript() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral.ToString());
            LogPrint(BCLog::MNBUDGET,"CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
            return false;
        }
        if (fBudgetFinalization) {
            // Collateral for budget finalization
            // Note: there are still old valid budgets out there, but the check for the new 5 SBE finalization collateral
            //       will also cover the old 50 SBE finalization collateral.
            LogPrint(BCLog::MNBUDGET, "Final Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::MNBUDGET, "Final Budget: o.nValue(%ld) >= BUDGET_FEE_TX(%ld) ?\n", o.nValue, BUDGET_FEE_TX);
                if(o.nValue >= BUDGET_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
        else {
            // Collateral for normal budget proposal
            LogPrint(BCLog::MNBUDGET, "Normal Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", HexStr(o.scriptPubKey), HexStr(findScript));
            if (o.scriptPubKey == findScript) {
                LogPrint(BCLog::MNBUDGET, "Normal Budget: o.nValue(%ld) >= PROPOSAL_FEE_TX(%ld) ?\n", o.nValue, PROPOSAL_FEE_TX);
                if(o.nValue >= PROPOSAL_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
    }
    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral.ToString());
        LogPrint(BCLog::MNBUDGET,"CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    // RETRIEVE CONFIRMATIONS AND NTIME
    /*
        - nTime starts as zero and is passed-by-reference out of this function and stored in the external proposal
        - nTime is never validated via the hashing mechanism and comes from a full-validated source (the blockchain)
    */

    int conf = GetIXConfirmations(nTxCollateralHash);
    if (!nBlockHash.IsNull()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
                nTime = pindex->nTime;
            }
        }
    }

    nConf = conf;

    //if we're syncing we won't have swiftTX information, so accept 1 confirmation
    const int nRequiredConfs = Params().GetConsensus().nBudgetFeeConfirmations;
    if (conf >= nRequiredConfs) {
        return true;
    } else {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations", nRequiredConfs, conf);
        LogPrint(BCLog::MNBUDGET,"CBudgetProposalBroadcast::IsBudgetCollateralValid - %s - %d confirmations\n", strError, conf);
        return false;
    }
}

void CBudgetManager::CheckOrphanVotes()
{
    LOCK(cs);


    std::string strError = "";
    std::map<uint256, CBudgetVote>::iterator it1 = mapOrphanMasternodeBudgetVotes.begin();
    while (it1 != mapOrphanMasternodeBudgetVotes.end()) {
        if (budget.UpdateProposal(((*it1).second), NULL, strError)) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanMasternodeBudgetVotes.erase(it1++);
        } else {
            ++it1;
        }
    }
    std::map<uint256, CFinalizedBudgetVote>::iterator it2 = mapOrphanFinalizedBudgetVotes.begin();
    while (it2 != mapOrphanFinalizedBudgetVotes.end()) {
        if (budget.UpdateFinalizedBudget(((*it2).second), NULL, strError)) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanFinalizedBudgetVotes.erase(it2++);
        } else {
            ++it2;
        }
    }
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckOrphanVotes - Done\n");
}

void CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final budget was submitted last time
    int nCurrentHeight;

    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        nCurrentHeight = chainActive.Height();
    }

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = nCurrentHeight - nCurrentHeight % nBlocksPerCycle + nBlocksPerCycle;
    if (nSubmittedHeight >= nBlockStart){
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - nSubmittedHeight(=%ld) < nBlockStart(=%ld) condition not fulfilled.\n", nSubmittedHeight, nBlockStart);
        return;
    }

     // Submit final budget during the last 2 days (2880 blocks) before payment for Mainnet, about 9 minutes (9 blocks) for Testnet
    int finalizationWindow = ((nBlocksPerCycle / 30) * 2);

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        // NOTE: 9 blocks for testnet is way to short to have any masternode submit an automatic vote on the finalized(!) budget,
        //       because those votes are only submitted/relayed once every 56 blocks in CFinalizedBudget::AutoCheck()

        finalizationWindow = 64; // 56 + 4 finalization confirmations + 4 minutes buffer for propagation
    }

    int nFinalizationStart = nBlockStart - finalizationWindow;

    int nOffsetToStart = nFinalizationStart - nCurrentHeight;

    if (nBlockStart - nCurrentHeight > finalizationWindow) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Too early for finalization. Current block is %ld, next Superblock is %ld.\n", nCurrentHeight, nBlockStart);
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - First possible block for finalization: %ld. Last possible block for finalization: %ld. You have to wait for %ld block(s) until Budget finalization will be possible\n", nFinalizationStart, nBlockStart, nOffsetToStart);

        return;
    }

    std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();
    std::string strBudgetName = "main";
    std::vector<CTxBudgetPayment> vecTxBudgetPayments;

    for (unsigned int i = 0; i < vBudgetProposals.size(); i++) {
        CTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = vBudgetProposals[i]->GetHash();
        txBudgetPayment.payee = vBudgetProposals[i]->GetPayee();
        txBudgetPayment.nAmount = vBudgetProposals[i]->GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.size() < 1) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Found No Proposals For Period\n");
        return;
    }

    CFinalizedBudgetBroadcast tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, UINT256_ZERO);
    if (mapSeenFinalizedBudgets.count(tempBudget.GetHash())) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Budget already exists - %s\n", tempBudget.GetHash().ToString());
        nSubmittedHeight = nCurrentHeight;
        return; //already exists
    }

    //create fee tx
    CTransaction tx;
    uint256 txidCollateral;

    if (!mapCollateralTxids.count(tempBudget.GetHash())) {
        CWalletTx wtx;
        if (!pwalletMain->GetBudgetFinalizationCollateralTX(wtx, tempBudget.GetHash(), false)) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Can't make collateral transaction\n");
            return;
        }

        // Get our change address
        CReserveKey reservekey(pwalletMain);
        // Send the tx to the network. Do NOT use SwiftTx, locking might need too much time to propagate, especially for testnet
        pwalletMain->CommitTransaction(wtx, reservekey, "NO-ix");
        tx = (CTransaction)wtx;
        txidCollateral = tx.GetHash();
        mapCollateralTxids.insert(std::make_pair(tempBudget.GetHash(), txidCollateral));
    } else {
        txidCollateral = mapCollateralTxids[tempBudget.GetHash()];
    }

    int conf = GetIXConfirmations(txidCollateral);
    CTransaction txCollateral;
    uint256 nBlockHash;

    if (!GetTransaction(txidCollateral, txCollateral, nBlockHash, true)) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Can't find collateral tx %s", txidCollateral.ToString());
        return;
    }

    if (!nBlockHash.IsNull()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
            }
        }
    }

    /*
        Wait will we have 1 extra confirmation, otherwise some clients might reject this feeTX
        -- This function is tied to NewBlock, so we will propagate this budget while the block is also propagating
    */
    const int nRequiredConfs = Params().GetConsensus().nBudgetFeeConfirmations;
    if (conf < nRequiredConfs + 1) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Collateral requires at least %d confirmations - %s - %d confirmations\n", nRequiredConfs + 1, txidCollateral.ToString(), conf);
        return;
    }

    //create the proposal incase we're the first to make it
    CFinalizedBudgetBroadcast finalizedBudgetBroadcast(strBudgetName, nBlockStart, vecTxBudgetPayments, txidCollateral);

    std::string strError = "";
    if (!finalizedBudgetBroadcast.IsValid(strError)) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Invalid finalized budget - %s \n", strError);
        return;
    }

    LOCK(cs);
    mapSeenFinalizedBudgets.insert(std::make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));
    finalizedBudgetBroadcast.Relay();
    budget.AddFinalizedBudget(finalizedBudgetBroadcast);
    nSubmittedHeight = nCurrentHeight;
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::SubmitFinalBudget - Done! %s\n", finalizedBudgetBroadcast.GetHash().ToString());
}

//
// CBudgetDB
//

CBudgetDB::CBudgetDB()
{
    pathDB = GetDataDir() / "budget.dat";
    strMagicMessage = "MasternodeBudget";
}

bool CBudgetDB::Write(const CBudgetManager& objToSave)
{
    LOCK(objToSave.cs);

    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // masternode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathDB, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathDB.string());

    // Write and commit header, data
    try {
        fileout << ssObj;
    } catch (const std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    fileout.fclose();

    LogPrint(BCLog::MNBUDGET,"Written info to budget.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CBudgetDB::ReadResult CBudgetDB::Read(CBudgetManager& objToLoad, bool fDryRun)
{
    LOCK(objToLoad.cs);

    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fsbridge::fopen(pathDB, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathDB.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = fs::file_size(pathDB);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    } catch (const std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssObj(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssObj.begin(), ssObj.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }


    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (masternode cache file specific magic message) and ..
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid masternode cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CBudgetManager object
        ssObj >> objToLoad;
    } catch (const std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint(BCLog::MNBUDGET,"Loaded info from budget.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint(BCLog::MNBUDGET,"  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrint(BCLog::MNBUDGET,"Budget manager - cleaning....\n");
        objToLoad.CheckAndRemove();
        LogPrint(BCLog::MNBUDGET,"Budget manager - result:\n");
        LogPrint(BCLog::MNBUDGET,"  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpBudgets()
{
    int64_t nStart = GetTimeMillis();

    CBudgetDB budgetdb;
    CBudgetManager tempBudget;

    LogPrint(BCLog::MNBUDGET,"Verifying budget.dat format...\n");
    CBudgetDB::ReadResult readResult = budgetdb.Read(tempBudget, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CBudgetDB::FileError)
        LogPrint(BCLog::MNBUDGET,"Missing budgets file - budget.dat, will try to recreate\n");
    else if (readResult != CBudgetDB::Ok) {
        LogPrint(BCLog::MNBUDGET,"Error reading budget.dat: ");
        if (readResult == CBudgetDB::IncorrectFormat)
            LogPrint(BCLog::MNBUDGET,"magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint(BCLog::MNBUDGET,"file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint(BCLog::MNBUDGET,"Writting info to budget.dat...\n");
    budgetdb.Write(budget);

    LogPrint(BCLog::MNBUDGET,"Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool CBudgetManager::AddFinalizedBudget(CFinalizedBudget& finalizedBudget)
{
    std::string strError = "";
    if (!finalizedBudget.IsValid(strError)) return false;

    if (mapFinalizedBudgets.count(finalizedBudget.GetHash())) {
        return false;
    }

    mapFinalizedBudgets.insert(std::make_pair(finalizedBudget.GetHash(), finalizedBudget));
    return true;
}

bool CBudgetManager::AddProposal(CBudgetProposal& budgetProposal)
{
    LOCK(cs);
    std::string strError = "";
    if (!budgetProposal.IsValid(strError)) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::AddProposal - invalid budget proposal - %s\n", strError);
        return false;
    }

    if (mapProposals.count(budgetProposal.GetHash())) {
        return false;
    }

    mapProposals.insert(std::make_pair(budgetProposal.GetHash(), budgetProposal));
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::AddProposal - proposal %s added\n", budgetProposal.GetName ().c_str ());
    return true;
}

void CBudgetManager::CheckAndRemove()
{
    int nHeight = 0;

    // Add some verbosity once loading blocks from files has finished
    {
        TRY_LOCK(cs_main, locked);
        if ((locked) && (chainActive.Tip() != NULL)) {
            CBlockIndex* pindexPrev = chainActive.Tip();
            if (pindexPrev) {
                nHeight = pindexPrev->nHeight;
            }
        }
    }

    LogPrint(BCLog::MNBUDGET, "CBudgetManager::CheckAndRemove at Height=%d\n", nHeight);

    std::map<uint256, CFinalizedBudget> tmpMapFinalizedBudgets;
    std::map<uint256, CBudgetProposal> tmpMapProposals;

    std::string strError = "";

    LogPrint(BCLog::MNBUDGET, "CBudgetManager::CheckAndRemove - mapFinalizedBudgets cleanup - size before: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        pfinalizedBudget->fValid = pfinalizedBudget->IsValid(strError);
        if (!strError.empty ()) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckAndRemove - Invalid finalized budget: %s\n", strError);
        }
        else {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckAndRemove - Found valid finalized budget: %s %s\n",
                      pfinalizedBudget->strBudgetName.c_str(), pfinalizedBudget->nFeeTXHash.ToString().c_str());
        }

        if (pfinalizedBudget->fValid) {
            pfinalizedBudget->CheckAndVote();
            tmpMapFinalizedBudgets.insert(std::make_pair(pfinalizedBudget->GetHash(), *pfinalizedBudget));
        }

        ++it;
    }

    LogPrint(BCLog::MNBUDGET, "CBudgetManager::CheckAndRemove - mapProposals cleanup - size before: %d\n", mapProposals.size());
    std::map<uint256, CBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        CBudgetProposal* pbudgetProposal = &((*it2).second);
        pbudgetProposal->fValid = pbudgetProposal->IsValid(strError);
        if (!strError.empty ()) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckAndRemove - Invalid budget proposal - %s\n", strError);
            strError = "";
        }
        else {
             LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckAndRemove - Found valid budget proposal: %s %s\n",
                      pbudgetProposal->strProposalName.c_str(), pbudgetProposal->nFeeTXHash.ToString().c_str());
        }
        if (pbudgetProposal->fValid) {
            tmpMapProposals.insert(std::make_pair(pbudgetProposal->GetHash(), *pbudgetProposal));
        }

        ++it2;
    }
    // Remove invalid entries by overwriting complete map
    mapFinalizedBudgets.swap(tmpMapFinalizedBudgets);
    mapProposals.swap(tmpMapProposals);

    // clang doesn't accept copy assignemnts :-/
    // mapFinalizedBudgets = tmpMapFinalizedBudgets;
    // mapProposals = tmpMapProposals;

    LogPrint(BCLog::MNBUDGET, "CBudgetManager::CheckAndRemove - mapFinalizedBudgets cleanup - size after: %d\n", mapFinalizedBudgets.size());
    LogPrint(BCLog::MNBUDGET, "CBudgetManager::CheckAndRemove - mapProposals cleanup - size after: %d\n", mapProposals.size());
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::CheckAndRemove - PASSED\n");

}

void CBudgetManager::FillBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake)
{
    LOCK(cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    int nHighestCount = 0;
    CScript payee;
    CAmount nAmount = 0;

    // ------- Grab The Highest Count

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            pindexPrev->nHeight + 1 >= pfinalizedBudget->GetBlockStart() &&
            pindexPrev->nHeight + 1 <= pfinalizedBudget->GetBlockEnd() &&
            pfinalizedBudget->GetPayeeAndAmount(pindexPrev->nHeight + 1, payee, nAmount)) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    CAmount blockValue = GetBlockValue(pindexPrev->nHeight);

    if (fProofOfStake) {
        if (nHighestCount > 0) {
            unsigned int i = txNew.vout.size();
            txNew.vout.resize(i + 1);
            txNew.vout[i].scriptPubKey = payee;
            txNew.vout[i].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::FillBlockPayee - Budget payment to %s for %lld, nHighestCount = %d\n", EncodeDestination(address1), nAmount, nHighestCount);
        }
        else {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::FillBlockPayee - No Budget payment, nHighestCount = %d\n", nHighestCount);
        }
    } else {
        //miners get the full amount on these blocks
        txNew.vout[0].nValue = blockValue;

        if (nHighestCount > 0) {
            txNew.vout.resize(2);

            //these are super blocks, so their value can be much larger than normal
            txNew.vout[1].scriptPubKey = payee;
            txNew.vout[1].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::FillBlockPayee - Budget payment to %s for %lld\n", EncodeDestination(address1), nAmount);
        }
    }
}

CFinalizedBudget* CBudgetManager::FindFinalizedBudget(uint256 nHash)
{
    if (mapFinalizedBudgets.count(nHash))
        return &mapFinalizedBudgets[nHash];

    return NULL;
}

CBudgetProposal* CBudgetManager::FindProposal(const std::string& strProposalName)
{
    //find the prop with the highest yes count

    int nYesCount = -99999;
    CBudgetProposal* pbudgetProposal = NULL;

    std::map<uint256, CBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        if ((*it).second.strProposalName == strProposalName && (*it).second.GetYeas() > nYesCount) {
            pbudgetProposal = &((*it).second);
            nYesCount = pbudgetProposal->GetYeas();
        }
        ++it;
    }

    if (nYesCount == -99999) return NULL;

    return pbudgetProposal;
}

CBudgetProposal* CBudgetManager::FindProposal(uint256 nHash)
{
    LOCK(cs);

    if (mapProposals.count(nHash))
        return &mapProposals[nHash];

    return NULL;
}

bool CBudgetManager::IsBudgetPaymentBlock(int nBlockHeight)
{
    int nHighestCount = -1;
    int nFivePercent = mnodeman.CountEnabled(ActiveProtocol()) / 20;

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsBudgetPaymentBlock() - nHighestCount: %lli, 5%% of Masternodes: %lli. Number of finalized budgets: %lli\n",
              nHighestCount, nFivePercent, mapFinalizedBudgets.size());

    // If budget doesn't have 5% of the network votes, then we should pay a masternode instead
    if (nHighestCount > nFivePercent) return true;

    return false;
}

TrxValidationStatus CBudgetManager::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs);

    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;
    int nHighestCount = 0;
    int nFivePercent = mnodeman.CountEnabled(ActiveProtocol()) / 20;
    std::vector<CFinalizedBudget*> ret;

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - checking %lli finalized budgets\n", mapFinalizedBudgets.size());

    // ------- Grab The Highest Count

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        if (pfinalizedBudget->GetVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid() - nHighestCount: %lli, 5%% of Masternodes: %lli mapFinalizedBudgets.size(): %ld\n",
              nHighestCount, nFivePercent, mapFinalizedBudgets.size());
    /*
        If budget doesn't have 5% of the network votes, then we should pay a masternode instead
    */
    if (nHighestCount < nFivePercent) return TrxValidationStatus::InValid;

    // check the highest finalized budgets (+/- 10% to assist in consensus)

    std::string strProposals = "";
    int nCountThreshold = nHighestCount - mnodeman.CountEnabled(ActiveProtocol()) / 10;
    bool fThreshold = false;
    it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        strProposals = pfinalizedBudget->GetProposals();

        LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - checking budget (%s) with blockstart %lli, blockend %lli, nBlockHeight %lli, votes %lli, nCountThreshold %lli\n",
                 strProposals.c_str(), pfinalizedBudget->GetBlockStart(), pfinalizedBudget->GetBlockEnd(),
                 nBlockHeight, pfinalizedBudget->GetVoteCount(), nCountThreshold);

        if (pfinalizedBudget->GetVoteCount() > nCountThreshold) {
            fThreshold = true;
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - GetVoteCount() > nCountThreshold passed\n");
            if (nBlockHeight >= pfinalizedBudget->GetBlockStart() && nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
                LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - GetBlockStart() passed\n");
                transactionStatus = pfinalizedBudget->IsTransactionValid(txNew, nBlockHeight);
                if (transactionStatus == TrxValidationStatus::Valid) {
                    LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - pfinalizedBudget->IsTransactionValid() passed\n");
                    return TrxValidationStatus::Valid;
                }
                else {
                    LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - pfinalizedBudget->IsTransactionValid() error\n");
                }
            }
            else {
                LogPrint(BCLog::MNBUDGET,"CBudgetManager::IsTransactionValid - GetBlockStart() failed, budget is outside current payment cycle and will be ignored.\n");
            }

        }

        ++it;
    }

    // If not enough masternodes autovoted for any of the finalized budgets pay a masternode instead
    if(!fThreshold) {
        transactionStatus = TrxValidationStatus::VoteThreshold;
    }

    // We looked through all of the known budgets
    return transactionStatus;
}

std::vector<CBudgetProposal*> CBudgetManager::GetAllProposals()
{
    LOCK(cs);

    std::vector<CBudgetProposal*> vBudgetProposalRet;

    std::map<uint256, CBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        (*it).second.CleanAndRemove();

        CBudgetProposal* pbudgetProposal = &((*it).second);
        vBudgetProposalRet.push_back(pbudgetProposal);

        ++it;
    }

    return vBudgetProposalRet;
}

//
// Sort by votes, if there's a tie sort by their feeHash TX
//
struct sortProposalsByVotes {
    bool operator()(const std::pair<CBudgetProposal*, int>& left, const std::pair<CBudgetProposal*, int>& right)
    {
        if (left.second != right.second)
            return (left.second > right.second);
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

//Need to review this function
std::vector<CBudgetProposal*> CBudgetManager::GetBudget()
{
    LOCK(cs);

    // ------- Sort budgets by Yes Count

    std::vector<std::pair<CBudgetProposal*, int> > vBudgetPorposalsSort;

    std::map<uint256, CBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        (*it).second.CleanAndRemove();
        vBudgetPorposalsSort.push_back(std::make_pair(&((*it).second), (*it).second.GetYeas() - (*it).second.GetNays()));
        ++it;
    }

    std::sort(vBudgetPorposalsSort.begin(), vBudgetPorposalsSort.end(), sortProposalsByVotes());

    // ------- Grab The Budgets In Order

    std::vector<CBudgetProposal*> vBudgetProposalsRet;

    CAmount nBudgetAllocated = 0;

    CBlockIndex* pindexPrev;
    {
        LOCK(cs_main);
        pindexPrev = chainActive.Tip();
    }
    if (pindexPrev == NULL) return vBudgetProposalsRet;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nBlockStart = pindexPrev->nHeight - pindexPrev->nHeight % nBlocksPerCycle + nBlocksPerCycle;
    int nBlockEnd = nBlockStart + nBlocksPerCycle - 1;
    int mnCount = mnodeman.CountEnabled(ActiveProtocol());
    CAmount nTotalBudget = GetTotalBudget(nBlockStart);

    std::vector<std::pair<CBudgetProposal*, int> >::iterator it2 = vBudgetPorposalsSort.begin();
    while (it2 != vBudgetPorposalsSort.end()) {
        CBudgetProposal* pbudgetProposal = (*it2).first;

        LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetBudget() - Processing Budget %s\n", pbudgetProposal->strProposalName.c_str());
        //prop start/end should be inside this period
        if (pbudgetProposal->IsPassing(pindexPrev, nBlockStart, nBlockEnd, mnCount)) {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetBudget() -   Check 1 passed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                      pbudgetProposal->fValid, pbudgetProposal->nBlockStart, nBlockStart, pbudgetProposal->nBlockEnd,
                      nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), mnCount / 10,
                      pbudgetProposal->IsEstablished());

            if (pbudgetProposal->GetAmount() + nBudgetAllocated <= nTotalBudget) {
                pbudgetProposal->SetAllotted(pbudgetProposal->GetAmount());
                nBudgetAllocated += pbudgetProposal->GetAmount();
                vBudgetProposalsRet.push_back(pbudgetProposal);
                LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetBudget() -     Check 2 passed: Budget added\n");
            } else {
                pbudgetProposal->SetAllotted(0);
                LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetBudget() -     Check 2 failed: no amount allotted\n");
            }
        }
        else {
            LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetBudget() -   Check 1 failed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                      pbudgetProposal->fValid, pbudgetProposal->nBlockStart, nBlockStart, pbudgetProposal->nBlockEnd,
                      nBlockEnd, pbudgetProposal->GetYeas(), pbudgetProposal->GetNays(), mnodeman.CountEnabled(ActiveProtocol()) / 10,
                      pbudgetProposal->IsEstablished());
        }

        ++it2;
    }

    return vBudgetProposalsRet;
}

// Sort by votes, if there's a tie sort by their feeHash TX
struct sortFinalizedBudgetsByVotes {
    bool operator()(const std::pair<CFinalizedBudget*, int>& left, const std::pair<CFinalizedBudget*, int>& right)
    {
        if (left.second != right.second)
            return left.second > right.second;
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

std::vector<CFinalizedBudget*> CBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs);

    std::vector<CFinalizedBudget*> vFinalizedBudgetsRet;
    std::vector<std::pair<CFinalizedBudget*, int> > vFinalizedBudgetsSort;

    // ------- Grab The Budgets In Order

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        vFinalizedBudgetsSort.push_back(std::make_pair(pfinalizedBudget, pfinalizedBudget->GetVoteCount()));
        ++it;
    }
    std::sort(vFinalizedBudgetsSort.begin(), vFinalizedBudgetsSort.end(), sortFinalizedBudgetsByVotes());

    std::vector<std::pair<CFinalizedBudget*, int> >::iterator it2 = vFinalizedBudgetsSort.begin();
    while (it2 != vFinalizedBudgetsSort.end()) {
        vFinalizedBudgetsRet.push_back((*it2).first);
        ++it2;
    }

    return vFinalizedBudgetsRet;
}

std::string CBudgetManager::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs);

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
                LogPrint(BCLog::MNBUDGET,"CBudgetManager::GetRequiredPaymentsString - Couldn't find budget payment for block %d\n", nBlockHeight);
            }
        }

        ++it;
    }

    return ret;
}

CAmount CBudgetManager::GetTotalBudget(int nHeight)
{
	return 1 * COIN;
}

void CBudgetManager::NewBlock()
{
    TRY_LOCK(cs, fBudgetNewBlock);
    if (!fBudgetNewBlock) return;

    if (masternodeSync.RequestedMasternodeAssets <= MASTERNODE_SYNC_BUDGET) return;

    if (strBudgetMode == "suggest") { //suggest the budget we see
        SubmitFinalBudget();
    }

    //this function should be called 1/14 blocks, allowing up to 100 votes per day on all proposals
    if (chainActive.Height() % 14 != 0) return;

    // incremental sync with our peers
    if (masternodeSync.IsSynced()) {
        LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - incremental sync started\n");
        if (chainActive.Height() % 1440 == rand() % 1440) {
            ClearSeen();
            ResetSync();
        }

        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->nVersion >= ActiveProtocol())
                Sync(pnode, UINT256_ZERO, true);

        MarkSynced();
    }


    CheckAndRemove();

    //remove invalid votes once in a while (we have to check the signatures and validity of every vote, somewhat CPU intensive)

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - askedForSourceProposalOrBudget cleanup - size: %d\n", askedForSourceProposalOrBudget.size());
    std::map<uint256, int64_t>::iterator it = askedForSourceProposalOrBudget.begin();
    while (it != askedForSourceProposalOrBudget.end()) {
        if ((*it).second > GetTime() - (60 * 60 * 24)) {
            ++it;
        } else {
            askedForSourceProposalOrBudget.erase(it++);
        }
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - mapProposals cleanup - size: %d\n", mapProposals.size());
    std::map<uint256, CBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        (*it2).second.CleanAndRemove();
        ++it2;
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - mapFinalizedBudgets cleanup - size: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CFinalizedBudget>::iterator it3 = mapFinalizedBudgets.begin();
    while (it3 != mapFinalizedBudgets.end()) {
        (*it3).second.CleanAndRemove();
        ++it3;
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - vecImmatureBudgetProposals cleanup - size: %d\n", vecImmatureBudgetProposals.size());
    std::vector<CBudgetProposalBroadcast>::iterator it4 = vecImmatureBudgetProposals.begin();
    while (it4 != vecImmatureBudgetProposals.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid((*it4).nFeeTXHash, (*it4).GetHash(), strError, (*it4).nTime, nConf)) {
            ++it4;
            continue;
        }

        if (!(*it4).IsValid(strError)) {
            LogPrint(BCLog::MNBUDGET,"mprop (immature) - invalid budget proposal - %s\n", strError);
            it4 = vecImmatureBudgetProposals.erase(it4);
            continue;
        }

        CBudgetProposal budgetProposal((*it4));
        if (AddProposal(budgetProposal)) {
            (*it4).Relay();
        }

        LogPrint(BCLog::MNBUDGET,"mprop (immature) - new budget - %s\n", (*it4).GetHash().ToString());
        it4 = vecImmatureBudgetProposals.erase(it4);
    }

    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - vecImmatureFinalizedBudgets cleanup - size: %d\n", vecImmatureFinalizedBudgets.size());
    std::vector<CFinalizedBudgetBroadcast>::iterator it5 = vecImmatureFinalizedBudgets.begin();
    while (it5 != vecImmatureFinalizedBudgets.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid((*it5).nFeeTXHash, (*it5).GetHash(), strError, (*it5).nTime, nConf, true)) {
            ++it5;
            continue;
        }

        if (!(*it5).IsValid(strError)) {
            LogPrint(BCLog::MNBUDGET,"fbs (immature) - invalid finalized budget - %s\n", strError);
            it5 = vecImmatureFinalizedBudgets.erase(it5);
            continue;
        }

        LogPrint(BCLog::MNBUDGET,"fbs (immature) - new finalized budget - %s\n", (*it5).GetHash().ToString());

        CFinalizedBudget finalizedBudget((*it5));
        if (AddFinalizedBudget(finalizedBudget)) {
            (*it5).Relay();
        }

        it5 = vecImmatureFinalizedBudgets.erase(it5);
    }
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::NewBlock - PASSED\n");
}

void CBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if (fLiteMode) return;
    if (!masternodeSync.IsBlockchainSynced()) return;

    LOCK(cs_budget);

    if (strCommand == NetMsgType::BUDGETVOTESYNC) { //Masternode vote sync
        uint256 nProp;
        vRecv >> nProp;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (nProp.IsNull()) {
                if (pfrom->HasFulfilledRequest("budgetvotesync")) {
                    LogPrint(BCLog::MNBUDGET,"mnvs - peer already asked me for the list\n");
                    Misbehaving(pfrom->GetId(), 20);
                    return;
                }
                pfrom->FulfilledRequest("budgetvotesync");
            }
        }

        Sync(pfrom, nProp);
        LogPrint(BCLog::MNBUDGET, "mnvs - Sent Masternode votes to peer %i\n", pfrom->GetId());
    }

    if (strCommand == NetMsgType::BUDGETPROPOSAL) { //Masternode Proposal
        CBudgetProposalBroadcast budgetProposalBroadcast;
        vRecv >> budgetProposalBroadcast;

        if (mapSeenMasternodeBudgetProposals.count(budgetProposalBroadcast.GetHash())) {
            masternodeSync.AddedBudgetItem(budgetProposalBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid(budgetProposalBroadcast.nFeeTXHash, budgetProposalBroadcast.GetHash(), strError, budgetProposalBroadcast.nTime, nConf)) {
            LogPrint(BCLog::MNBUDGET,"Proposal FeeTX is not valid - %s - %s\n", budgetProposalBroadcast.nFeeTXHash.ToString(), strError);
            if (nConf >= 1) vecImmatureBudgetProposals.push_back(budgetProposalBroadcast);
            return;
        }

        mapSeenMasternodeBudgetProposals.insert(std::make_pair(budgetProposalBroadcast.GetHash(), budgetProposalBroadcast));

        if (!budgetProposalBroadcast.IsValid(strError)) {
            LogPrint(BCLog::MNBUDGET,"mprop - invalid budget proposal - %s\n", strError);
            return;
        }

        CBudgetProposal budgetProposal(budgetProposalBroadcast);
        if (AddProposal(budgetProposal)) {
            budgetProposalBroadcast.Relay();
        }
        masternodeSync.AddedBudgetItem(budgetProposalBroadcast.GetHash());

        LogPrint(BCLog::MNBUDGET,"mprop - new budget - %s\n", budgetProposalBroadcast.GetHash().ToString());

        //We might have active votes for this proposal that are valid now
        CheckOrphanVotes();
    }

    if (strCommand == NetMsgType::BUDGETVOTE) { // Budget Vote
        CBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenMasternodeBudgetVotes.count(vote.GetHash())) {
            masternodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CMasternode* pmn = mnodeman.Find(vote.vin);
        if (pmn == NULL) {
            LogPrint(BCLog::MNBUDGET,"mvote - unknown masternode - vin: %s\n", vote.vin.prevout.hash.ToString());
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }


        mapSeenMasternodeBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
        if (!vote.CheckSignature()) {
            if (masternodeSync.IsSynced()) {
                LogPrintf("CBudgetManager::ProcessMessage() : mvote - signature invalid\n");
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced masternode
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        std::string strError = "";
        if (UpdateProposal(vote, pfrom, strError)) {
            vote.Relay();
            masternodeSync.AddedBudgetItem(vote.GetHash());
        }

        LogPrint(BCLog::MNBUDGET,"mvote - new budget vote for budget %s - %s\n", vote.nProposalHash.ToString(),  vote.GetHash().ToString());
    }

    if (strCommand == NetMsgType::FINALBUDGET) { //Finalized Budget Suggestion
        CFinalizedBudgetBroadcast finalizedBudgetBroadcast;
        vRecv >> finalizedBudgetBroadcast;

        if (mapSeenFinalizedBudgets.count(finalizedBudgetBroadcast.GetHash())) {
            masternodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid(finalizedBudgetBroadcast.nFeeTXHash, finalizedBudgetBroadcast.GetHash(), strError, finalizedBudgetBroadcast.nTime, nConf, true)) {
            LogPrint(BCLog::MNBUDGET,"fbs - Finalized Budget FeeTX is not valid - %s - %s\n", finalizedBudgetBroadcast.nFeeTXHash.ToString(), strError);

            if (nConf >= 1) vecImmatureFinalizedBudgets.push_back(finalizedBudgetBroadcast);
            return;
        }

        mapSeenFinalizedBudgets.insert(std::make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));

        if (!finalizedBudgetBroadcast.IsValid(strError)) {
            LogPrint(BCLog::MNBUDGET,"fbs - invalid finalized budget - %s\n", strError);
            return;
        }

        LogPrint(BCLog::MNBUDGET,"fbs - new finalized budget - %s\n", finalizedBudgetBroadcast.GetHash().ToString());

        CFinalizedBudget finalizedBudget(finalizedBudgetBroadcast);
        if (AddFinalizedBudget(finalizedBudget)) {
            finalizedBudgetBroadcast.Relay();
        }
        masternodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());

        //we might have active votes for this budget that are now valid
        CheckOrphanVotes();
    }

    if (strCommand == NetMsgType::FINALBUDGETVOTE) { //Finalized Budget Vote
        CFinalizedBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenFinalizedBudgetVotes.count(vote.GetHash())) {
            masternodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CMasternode* pmn = mnodeman.Find(vote.vin);
        if (pmn == NULL) {
            LogPrint(BCLog::MNBUDGET, "fbvote - unknown masternode - vin: %s\n", vote.vin.prevout.hash.ToString());
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        mapSeenFinalizedBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
        if (!vote.CheckSignature()) {
            if (masternodeSync.IsSynced()) {
                LogPrintf("CBudgetManager::ProcessMessage() : fbvote - signature from masternode %s invalid\n", HexStr(pmn->pubKeyMasternode));
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced masternode
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        std::string strError = "";
        if (UpdateFinalizedBudget(vote, pfrom, strError)) {
            vote.Relay();
            masternodeSync.AddedBudgetItem(vote.GetHash());

            LogPrint(BCLog::MNBUDGET,"fbvote - new finalized budget vote - %s from masternode %s\n", vote.GetHash().ToString(), HexStr(pmn->pubKeyMasternode));
        } else {
            LogPrint(BCLog::MNBUDGET,"fbvote - rejected finalized budget vote - %s from masternode %s - %s\n", vote.GetHash().ToString(), HexStr(pmn->pubKeyMasternode), strError);
        }
    }
}

bool CBudgetManager::PropExists(uint256 nHash)
{
    if (mapProposals.count(nHash)) return true;
    return false;
}

//mark that a full sync is needed
void CBudgetManager::ResetSync()
{
    LOCK(cs);


    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenMasternodeBudgetProposals.begin();
    while (it1 != mapSeenMasternodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                (*it2).second.fSynced = false;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //send votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                (*it4).second.fSynced = false;
                ++it4;
            }
        }
        ++it3;
    }
}

void CBudgetManager::MarkSynced()
{
    LOCK(cs);

    /*
        Mark that we've sent all valid items
    */

    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenMasternodeBudgetProposals.begin();
    while (it1 != mapSeenMasternodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid)
                    (*it2).second.fSynced = true;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //mark votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid)
                    (*it4).second.fSynced = true;
                ++it4;
            }
        }
        ++it3;
    }
}


void CBudgetManager::Sync(CNode* pfrom, uint256 nProp, bool fPartial)
{
    LOCK(cs);

    /*
        Sync with a client on the network

        --

        This code checks each of the hash maps for all known budget proposals and finalized budget proposals, then checks them against the
        budget object to see if they're OK. If all checks pass, we'll send it to the peer.

    */

    int nInvCount = 0;

    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenMasternodeBudgetProposals.begin();
    while (it1 != mapSeenMasternodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid && (nProp.IsNull() || (*it1).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_PROPOSAL, (*it1).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid) {
                    if ((fPartial && !(*it2).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_BUDGET_VOTE, (*it2).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it2;
            }
        }
        ++it1;
    }

    pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_BUDGET_PROP, nInvCount);

    LogPrint(BCLog::MNBUDGET, "CBudgetManager::Sync - sent %d items\n", nInvCount);

    nInvCount = 0;

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid && (nProp.IsNull() || (*it3).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED, (*it3).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid) {
                    if ((fPartial && !(*it4).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED_VOTE, (*it4).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it4;
            }
        }
        ++it3;
    }

    pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_BUDGET_FIN, nInvCount);
    LogPrint(BCLog::MNBUDGET, "CBudgetManager::Sync - sent %d items\n", nInvCount);
}

bool CBudgetManager::UpdateProposal(CBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapProposals.count(vote.nProposalHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!masternodeSync.IsSynced()) return false;

            LogPrint(BCLog::MNBUDGET,"CBudgetManager::UpdateProposal - Unknown proposal %d, asking for source proposal\n", vote.nProposalHash.ToString());
            mapOrphanMasternodeBudgetVotes[vote.nProposalHash] = vote;

            if (!askedForSourceProposalOrBudget.count(vote.nProposalHash)) {
                pfrom->PushMessage(NetMsgType::BUDGETVOTESYNC, vote.nProposalHash);
                askedForSourceProposalOrBudget[vote.nProposalHash] = GetTime();
            }
        }

        strError = "Proposal not found!";
        return false;
    }


    return mapProposals[vote.nProposalHash].AddOrUpdateVote(vote, strError);
}

bool CBudgetManager::UpdateFinalizedBudget(CFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapFinalizedBudgets.count(vote.nBudgetHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!masternodeSync.IsSynced()) return false;

            LogPrint(BCLog::MNBUDGET,"CBudgetManager::UpdateFinalizedBudget - Unknown Finalized Proposal %s, asking for source budget\n", vote.nBudgetHash.ToString());
            mapOrphanFinalizedBudgetVotes[vote.nBudgetHash] = vote;

            if (!askedForSourceProposalOrBudget.count(vote.nBudgetHash)) {
                pfrom->PushMessage(NetMsgType::BUDGETVOTESYNC, vote.nBudgetHash);
                askedForSourceProposalOrBudget[vote.nBudgetHash] = GetTime();
            }
        }

        strError = "Finalized Budget " + vote.nBudgetHash.ToString() +  " not found!";
        return false;
    }
    LogPrint(BCLog::MNBUDGET,"CBudgetManager::UpdateFinalizedBudget - Finalized Proposal %s added\n", vote.nBudgetHash.ToString());
    return mapFinalizedBudgets[vote.nBudgetHash].AddOrUpdateVote(vote, strError);
}

CBudgetProposal::CBudgetProposal()
{
    strProposalName = "unknown";
    nBlockStart = 0;
    nBlockEnd = 0;
    nAmount = 0;
    nTime = 0;
    fValid = true;
}

CBudgetProposal::CBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;
    nBlockEnd = nBlockEndIn;
    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
    fValid = true;
}

CBudgetProposal::CBudgetProposal(const CBudgetProposal& other)
{
    strProposalName = other.strProposalName;
    strURL = other.strURL;
    nBlockStart = other.nBlockStart;
    nBlockEnd = other.nBlockEnd;
    address = other.address;
    nAmount = other.nAmount;
    nTime = other.nTime;
    nFeeTXHash = other.nFeeTXHash;
    mapVotes = other.mapVotes;
    fValid = true;
}

bool CBudgetProposal::IsValid(std::string& strError, bool fCheckCollateral)
{
    if (GetNays() - GetYeas() > mnodeman.CountEnabled(ActiveProtocol()) / 10) {
        strError = "Proposal " + strProposalName + ": Active removal";
        return false;
    }

    if (nBlockStart < 0) {
        strError = "Invalid Proposal";
        return false;
    }

    if (nBlockEnd < nBlockStart) {
        strError = "Proposal " + strProposalName + ": Invalid nBlockEnd (end before start)";
        return false;
    }

    if (nAmount < 10 * COIN) {
        strError = "Proposal " + strProposalName + ": Invalid nAmount";
        return false;
    }

    if (address == CScript()) {
        strError = "Proposal " + strProposalName + ": Invalid Payment Address";
        return false;
    }

    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError, nTime, nConf)) {
            strError = "Proposal " + strProposalName + ": Invalid collateral";
            return false;
        }
    }

    /*
        TODO: There might be an issue with multisig in the coinbase on mainnet, we will add support for it in a future release.
    */
    if (address.IsPayToScriptHash()) {
        strError = "Proposal " + strProposalName + ": Multisig is not currently supported.";
        return false;
    }

    //if proposal doesn't gain traction within 2 weeks, remove it
    // nTime not being saved correctly
    // -- TODO: We should keep track of the last time the proposal was valid, if it's invalid for 2 weeks, erase it
    // if(nTime + (60*60*24*2) < GetAdjustedTime()) {
    //     if(GetYeas()-GetNays() < (mnodeman.CountEnabled(ActiveProtocol())/10)) {
    //         strError = "Not enough support";
    //         return false;
    //     }
    // }

    //can only pay out 10% of the possible coins (min value of coins)
    if (nAmount > budget.GetTotalBudget(nBlockStart)) {
        strError = "Proposal " + strProposalName + ": Payment more than max";
        return false;
    }

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) {
        strError = "Proposal " + strProposalName + ": Tip is NULL";
        return true;
    }

    // Calculate maximum block this proposal will be valid, which is start of proposal + (number of payments * cycle)
    int nProposalEnd = GetBlockStart() + (Params().GetConsensus().nBudgetCycleBlocks * GetTotalPaymentCount());

    // if (GetBlockEnd() < pindexPrev->nHeight - GetBudgetPaymentCycleBlocks() / 2) {
    if(nProposalEnd < pindexPrev->nHeight){
        strError = "Proposal " + strProposalName + ": Invalid nBlockEnd (" + std::to_string(nProposalEnd) + ") < current height (" + std::to_string(pindexPrev->nHeight) + ")";
        return false;
    }

    return true;
}

bool CBudgetProposal::IsEstablished()
{
    return nTime < GetAdjustedTime() - Params().GetConsensus().nProposalEstablishmentTime;
}

bool CBudgetProposal::IsPassing(const CBlockIndex* pindexPrev, int nBlockStartBudget, int nBlockEndBudget, int mnCount)
{
    if (!fValid)
        return false;

    if (!pindexPrev)
        return false;

    if (this->nBlockStart > nBlockStartBudget)
        return false;

    if (this->nBlockEnd < nBlockEndBudget)
        return false;

    if (GetYeas() - GetNays() <= mnCount / 10)
        return false;

    if (!IsEstablished())
        return false;

    return true;
}

bool CBudgetProposal::AddOrUpdateVote(CBudgetVote& vote, std::string& strError)
{
    std::string strAction = "New vote inserted:";
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();

    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint(BCLog::MNBUDGET, "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime,BUDGET_VOTE_UPDATE_MIN);
            LogPrint(BCLog::MNBUDGET, "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint(BCLog::MNBUDGET, "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint(BCLog::MNBUDGET, "CBudgetProposal::AddOrUpdateVote - %s %s\n", strAction.c_str(), vote.GetHash().ToString().c_str());

    return true;
}

// If masternode voted for a proposal, but is now invalid -- remove the vote
void CBudgetProposal::CleanAndRemove()
{
    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        CMasternode* pmn = mnodeman.Find((*it).second.GetVin());
        (*it).second.fValid = (pmn != nullptr);
        ++it;
    }
}

double CBudgetProposal::GetRatio()
{
    int yeas = 0;
    int nays = 0;

    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES) yeas++;
        if ((*it).second.nVote == VOTE_NO) nays++;
        ++it;
    }

    if (yeas + nays == 0) return 0.0f;

    return ((double)(yeas) / (double)(yeas + nays));
}

int CBudgetProposal::GetYeas() const
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::const_iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetNays() const
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::const_iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_NO && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetAbstains() const
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::const_iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_ABSTAIN && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetBlockStartCycle()
{
    //end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)

    return nBlockStart - nBlockStart % Params().GetConsensus().nBudgetCycleBlocks;
}

int CBudgetProposal::GetBlockCurrentCycle()
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return -1;

    if (pindexPrev->nHeight >= GetBlockEndCycle()) return -1;

    return pindexPrev->nHeight - pindexPrev->nHeight % Params().GetConsensus().nBudgetCycleBlocks;
}

int CBudgetProposal::GetBlockEndCycle()
{
    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    // return nBlockEnd - GetBudgetPaymentCycleBlocks() / 2;

    // End block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    return nBlockEnd;

}

int CBudgetProposal::GetTotalPaymentCount()
{
    return (GetBlockEndCycle() - GetBlockStartCycle()) / Params().GetConsensus().nBudgetCycleBlocks;
}

int CBudgetProposal::GetRemainingPaymentCount()
{
    // If this budget starts in the future, this value will be wrong
    int nPayments = (GetBlockEndCycle() - GetBlockCurrentCycle()) / Params().GetConsensus().nBudgetCycleBlocks - 1;
    // Take the lowest value
    return std::min(nPayments, GetTotalPaymentCount());
}

CBudgetProposalBroadcast::CBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;

    nBlockStart = nBlockStartIn;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nCycleStart = nBlockStart - nBlockStart % nBlocksPerCycle;

    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // calculate the end of the cycle for this vote, add half a cycle (vote will be deleted after that block)
    // nBlockEnd = nCycleStart + GetBudgetPaymentCycleBlocks() * nPaymentCount + GetBudgetPaymentCycleBlocks() / 2;

    // Calculate the end of the cycle for this vote, vote will be deleted after next cycle
    nBlockEnd = nCycleStart + (nBlocksPerCycle + 1)  * nPaymentCount;

    address = addressIn;
    nAmount = nAmountIn;

    nFeeTXHash = nFeeTXHashIn;
}

void CBudgetProposalBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_PROPOSAL, GetHash());
    RelayInv(inv);
}

CBudgetVote::CBudgetVote() :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(),
        nProposalHash(),
        nVote(VOTE_ABSTAIN),
        nTime(0)
{ }

CBudgetVote::CBudgetVote(CTxIn vinIn, uint256 nProposalHashIn, int nVoteIn) :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(vinIn),
        nProposalHash(nProposalHashIn),
        nVote(nVoteIn)
{
    nTime = GetAdjustedTime();
}

void CBudgetVote::Relay()
{
    CInv inv(MSG_BUDGET_VOTE, GetHash());
    RelayInv(inv);
}

uint256 CBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nProposalHash;
    ss << nVote;
    ss << nTime;
    return ss.GetHash();
}

std::string CBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nProposalHash.ToString() +
            std::to_string(nVote) + std::to_string(nTime);
}

CFinalizedBudget::CFinalizedBudget() :
        fAutoChecked(false),
        fValid(true),
        strBudgetName(""),
        nBlockStart(0),
        vecBudgetPayments(),
        mapVotes(),
        nFeeTXHash(),
        nTime(0)
{ }

CFinalizedBudget::CFinalizedBudget(const CFinalizedBudget& other) :
        fAutoChecked(false),
        fValid(true),
        strBudgetName(other.strBudgetName),
        nBlockStart(other.nBlockStart),
        vecBudgetPayments(other.vecBudgetPayments),
        mapVotes(other.mapVotes),
        nFeeTXHash(other.nFeeTXHash),
        nTime(other.nTime)
{ }

bool CFinalizedBudget::AddOrUpdateVote(CFinalizedBudgetVote& vote, std::string& strError)
{
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();
    std::string strAction = "New vote inserted:";

    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime,BUDGET_VOTE_UPDATE_MIN);
            LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::AddOrUpdateVote - %s %s\n", strAction.c_str(), vote.GetHash().ToString().c_str());
    return true;
}

// Sort budget proposals by hash
struct sortProposalsByHash  {
    bool operator()(const CBudgetProposal* left, const CBudgetProposal* right)
    {
        return (left->GetHash() < right->GetHash());
    }
};

// Sort budget payments by hash
struct sortPaymentsByHash  {
    bool operator()(const CTxBudgetPayment& left, const CTxBudgetPayment& right)
    {
        return (left.nProposalHash < right.nProposalHash);
    }
};

// Check finalized budget and vote on it if correct. Masternodes only
void CFinalizedBudget::CheckAndVote()
{
    LOCK(cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - %lli - %d\n", pindexPrev->nHeight, fAutoChecked);

    if (!fMasterNode || fAutoChecked) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck fMasterNode=%d fAutoChecked=%d\n", fMasterNode, fAutoChecked);
        return;
    }

    // Do this 1 in 4 blocks -- spread out the voting activity
    // -- this function is only called every fourteenth block, so this is really 1 in 56 blocks
    if (rand() % 4 != 0) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - waiting\n");
        return;
    }

    fAutoChecked = true; //we only need to check this once

    if (strBudgetMode == "auto") //only vote for exact matches
    {
        std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();

        // We have to resort the proposals by hash (they are sorted by votes here) and sort the payments
        // by hash (they are not sorted at all) to make the following tests deterministic
        // We're working on copies to avoid any side-effects by the possibly changed sorting order

        // Sort copy of proposals by hash
        std::vector<CBudgetProposal*> vBudgetProposalsSortedByHash(vBudgetProposals);
        std::sort(vBudgetProposalsSortedByHash.begin(), vBudgetProposalsSortedByHash.end(), sortProposalsByHash());

        // Sort copy payments by hash
        std::vector<CTxBudgetPayment> vecBudgetPaymentsSortedByHash(vecBudgetPayments);
        std::sort(vecBudgetPaymentsSortedByHash.begin(), vecBudgetPaymentsSortedByHash.end(), sortPaymentsByHash());

        for (unsigned int i = 0; i < vecBudgetPaymentsSortedByHash.size(); i++) {
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Payments - nProp %d %s\n", i, vecBudgetPaymentsSortedByHash[i].nProposalHash.ToString());
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Payments - Payee %d %s\n", i, HexStr(vecBudgetPaymentsSortedByHash[i].payee));
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Payments - nAmount %d %lli\n", i, vecBudgetPaymentsSortedByHash[i].nAmount);
        }

        for (unsigned int i = 0; i < vBudgetProposalsSortedByHash.size(); i++) {
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Proposals - nProp %d %s\n", i, vBudgetProposalsSortedByHash[i]->GetHash().ToString());
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Proposals - Payee %d %s\n", i, HexStr(vBudgetProposalsSortedByHash[i]->GetPayee()));
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck Budget-Proposals - nAmount %d %lli\n", i, vBudgetProposalsSortedByHash[i]->GetAmount());
        }

        if (vBudgetProposalsSortedByHash.size() == 0) {
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - No Budget-Proposals found, aborting\n");
            return;
        }

        if (vBudgetProposalsSortedByHash.size() != vecBudgetPaymentsSortedByHash.size()) {
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - Budget-Proposal length (%ld) doesn't match Budget-Payment length (%ld).\n",
                      vBudgetProposalsSortedByHash.size(), vecBudgetPaymentsSortedByHash.size());
            return;
        }

        for (unsigned int i = 0; i < vecBudgetPaymentsSortedByHash.size(); i++) {
            if (i > vBudgetProposalsSortedByHash.size() - 1) {
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - Proposal size mismatch, i=%d > (vBudgetProposals.size() - 1)=%d\n", i, vBudgetProposalsSortedByHash.size() - 1);
                return;
            }

            if (vecBudgetPaymentsSortedByHash[i].nProposalHash != vBudgetProposalsSortedByHash[i]->GetHash()) {
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - item #%d doesn't match %s %s\n", i, vecBudgetPaymentsSortedByHash[i].nProposalHash.ToString(), vBudgetProposalsSortedByHash[i]->GetHash().ToString());
                return;
            }

            // if(vecBudgetPayments[i].payee != vBudgetProposals[i]->GetPayee()){ -- triggered with false positive
            if (HexStr(vecBudgetPaymentsSortedByHash[i].payee) != HexStr(vBudgetProposalsSortedByHash[i]->GetPayee())) {
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - item #%d payee doesn't match %s %s\n", i, HexStr(vecBudgetPaymentsSortedByHash[i].payee), HexStr(vBudgetProposalsSortedByHash[i]->GetPayee()));
                return;
            }

            if (vecBudgetPaymentsSortedByHash[i].nAmount != vBudgetProposalsSortedByHash[i]->GetAmount()) {
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - item #%d payee doesn't match %lli %lli\n", i, vecBudgetPaymentsSortedByHash[i].nAmount, vBudgetProposalsSortedByHash[i]->GetAmount());
                return;
            }
        }

        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::AutoCheck - Finalized Budget Matches! Submitting Vote.\n");
        SubmitVote();
    }
}

// Remove votes from masternodes which are not valid/existent anymore
void CFinalizedBudget::CleanAndRemove()
{
    std::map<uint256, CFinalizedBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        CMasternode* pmn = mnodeman.Find((*it).second.GetVin());
        (*it).second.fValid = (pmn != nullptr);
        ++it;
    }
}

CAmount CFinalizedBudget::GetTotalPayout()
{
    CAmount ret = 0;

    for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
        ret += vecBudgetPayments[i].nAmount;
    }

    return ret;
}

std::string CFinalizedBudget::GetProposals()
{
    LOCK(cs);
    std::string ret = "";

    for (CTxBudgetPayment& budgetPayment : vecBudgetPayments) {
        CBudgetProposal* pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);

        std::string token = budgetPayment.nProposalHash.ToString();

        if (pbudgetProposal) token = pbudgetProposal->GetName();
        if (ret == "") {
            ret = token;
        } else {
            ret += "," + token;
        }
    }
    return ret;
}

std::string CFinalizedBudget::GetStatus()
{
    std::string retBadHashes = "";
    std::string retBadPayeeOrAmount = "";

    for (int nBlockHeight = GetBlockStart(); nBlockHeight <= GetBlockEnd(); nBlockHeight++) {
        CTxBudgetPayment budgetPayment;
        if (!GetBudgetPaymentByBlock(nBlockHeight, budgetPayment)) {
            LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::GetStatus - Couldn't find budget payment for block %lld\n", nBlockHeight);
            continue;
        }

        CBudgetProposal* pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);
        if (!pbudgetProposal) {
            if (retBadHashes == "") {
                retBadHashes = "Unknown proposal hash! Check this proposal before voting: " + budgetPayment.nProposalHash.ToString();
            } else {
                retBadHashes += "," + budgetPayment.nProposalHash.ToString();
            }
        } else {
            if (pbudgetProposal->GetPayee() != budgetPayment.payee || pbudgetProposal->GetAmount() != budgetPayment.nAmount) {
                if (retBadPayeeOrAmount == "") {
                    retBadPayeeOrAmount = "Budget payee/nAmount doesn't match our proposal! " + budgetPayment.nProposalHash.ToString();
                } else {
                    retBadPayeeOrAmount += "," + budgetPayment.nProposalHash.ToString();
                }
            }
        }
    }

    if (retBadHashes == "" && retBadPayeeOrAmount == "") return "OK";

    return retBadHashes + retBadPayeeOrAmount;
}

bool CFinalizedBudget::IsValid(std::string& strError, bool fCheckCollateral)
{
    // All(!) finalized budgets have the name "main", so get some additional information about them
    std::string strProposals = GetProposals();

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    // Must be the correct block for payment to happen (once a month)
    if (nBlockStart % nBlocksPerCycle != 0) {
        strError = "Invalid BlockStart";
        return false;
    }

    // The following 2 checks check the same (basically if vecBudgetPayments.size() > 100)
    if (GetBlockEnd() - nBlockStart > 100) {
        strError = "Invalid BlockEnd";
        return false;
    }
    if ((int)vecBudgetPayments.size() > 100) {
        strError = "Invalid budget payments count (too many)";
        return false;
    }
    if (strBudgetName == "") {
        strError = "Invalid Budget Name";
        return false;
    }
    if (nBlockStart == 0) {
        strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid BlockStart == 0";
        return false;
    }
    if (nFeeTXHash.IsNull()) {
        strError = "Budget " + strBudgetName  + " (" + strProposals + ") Invalid FeeTx == 0";
        return false;
    }

    // Can only pay out 10% of the possible coins (min value of coins)
    if (GetTotalPayout() > budget.GetTotalBudget(nBlockStart)) {
        strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Payout (more than max)";
        return false;
    }

    std::string strError2 = "";
    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError2, nTime, nConf, true)) {
            {
                strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Collateral : " + strError2;
                return false;
            }
        }
    }

    // Remove obsolete finalized budgets after some time

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return true;

    // Get start of current budget-cycle
    int nCurrentHeight = chainActive.Height();
    int nBlockStart = nCurrentHeight - nCurrentHeight % nBlocksPerCycle + nBlocksPerCycle;

    // Remove budgets where the last payment (from max. 100) ends before 2 budget-cycles before the current one
    int nMaxAge = nBlockStart - (2 * nBlocksPerCycle);

    if (GetBlockEnd() < nMaxAge) {
        strError = strprintf("Budget " + strBudgetName + " (" + strProposals + ") (ends at block %ld) too old and obsolete", GetBlockEnd());
        return false;
    }

    return true;
}

bool CFinalizedBudget::IsPaidAlready(uint256 nProposalHash, int nBlockHeight)
{
    // Remove budget-payments from former/future payment cycles
    std::map<uint256, int>::iterator it = mapPayment_History.begin();
    int nPaidBlockHeight = 0;
    uint256 nOldProposalHash;

    for(it = mapPayment_History.begin(); it != mapPayment_History.end(); /* No incrementation needed */ ) {
        nPaidBlockHeight = (*it).second;
        if((nPaidBlockHeight < GetBlockStart()) || (nPaidBlockHeight > GetBlockEnd())) {
            nOldProposalHash = (*it).first;
            LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::IsPaidAlready - Budget Proposal %s, Block %d from old cycle deleted\n",
                      nOldProposalHash.ToString().c_str(), nPaidBlockHeight);
            mapPayment_History.erase(it++);
        }
        else {
            ++it;
        }
    }

    // Now that we only have payments from the current payment cycle check if this budget was paid already
    if(mapPayment_History.count(nProposalHash) == 0) {
        // New proposal payment, insert into map for checks with later blocks from this cycle
        mapPayment_History.insert(std::pair<uint256, int>(nProposalHash, nBlockHeight));
        LogPrint(BCLog::MNBUDGET, "CFinalizedBudget::IsPaidAlready - Budget Proposal %s, Block %d added to payment history\n",
                  nProposalHash.ToString().c_str(), nBlockHeight);
        return false;
    }
    // This budget was paid already -> reject transaction so it gets paid to a masternode instead
    return true;
}

TrxValidationStatus CFinalizedBudget::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;
    int nCurrentBudgetPayment = nBlockHeight - GetBlockStart();
    if (nCurrentBudgetPayment < 0) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - Invalid block - height: %d start: %d\n", nBlockHeight, GetBlockStart());
        return TrxValidationStatus::InValid;
    }

    if (nCurrentBudgetPayment > (int)vecBudgetPayments.size() - 1) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - Invalid last block - current budget payment: %d of %d\n", nCurrentBudgetPayment + 1, (int)vecBudgetPayments.size());
        return TrxValidationStatus::InValid;
    }

    bool paid = false;

    for (CTxOut out : txNew.vout) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - nCurrentBudgetPayment=%d, payee=%s == out.scriptPubKey=%s, amount=%ld == out.nValue=%ld\n",
                 nCurrentBudgetPayment, HexStr(vecBudgetPayments[nCurrentBudgetPayment].payee), HexStr(out.scriptPubKey),
                 vecBudgetPayments[nCurrentBudgetPayment].nAmount, out.nValue);

        if (vecBudgetPayments[nCurrentBudgetPayment].payee == out.scriptPubKey && vecBudgetPayments[nCurrentBudgetPayment].nAmount == out.nValue) {
            // Check if this proposal was paid already. If so, pay a masternode instead
            paid = IsPaidAlready(vecBudgetPayments[nCurrentBudgetPayment].nProposalHash, nBlockHeight);
            if(paid) {
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - Double Budget Payment of %d for proposal %d detected. Paying a masternode instead.\n",
                          vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
                // No matter what we've found before, stop all checks here. In future releases there might be more than one budget payment
                // per block, so even if the first one was not paid yet this one disables all budget payments for this block.
                transactionStatus = TrxValidationStatus::DoublePayment;
                break;
            }
            else {
                transactionStatus = TrxValidationStatus::Valid;
                LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - Found valid Budget Payment of %d for proposal %d\n",
                          vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.GetHex());
            }
        }
    }

    if (transactionStatus == TrxValidationStatus::InValid) {
        CTxDestination address1;
        ExtractDestination(vecBudgetPayments[nCurrentBudgetPayment].payee, address1);

        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::IsTransactionValid - Missing required payment - %s: %d c: %d\n",
                  EncodeDestination(address1), vecBudgetPayments[nCurrentBudgetPayment].nAmount, nCurrentBudgetPayment);
    }

    return transactionStatus;
}

void CFinalizedBudget::SubmitVote()
{
    std::string strError = "";
    CPubKey pubKeyMasternode;
    CKey keyMasternode;

    bool fNewSigs = false;
    {
        LOCK(cs_main);
        fNewSigs = chainActive.NewSigsActive();
    }

    if (!CMessageSigner::GetKeysFromSecret(strMasterNodePrivKey, keyMasternode, pubKeyMasternode)) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::SubmitVote - Error upon calling GetKeysFromSecret\n");
        return;
    }

    CFinalizedBudgetVote vote(activeMasternode.vin, GetHash());
    if (!vote.Sign(keyMasternode, pubKeyMasternode, fNewSigs)) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::SubmitVote - Failure to sign.");
        return;
    }

    if (budget.UpdateFinalizedBudget(vote, NULL, strError)) {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::SubmitVote  - new finalized budget vote - %s\n", vote.GetHash().ToString());

        budget.mapSeenFinalizedBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
        vote.Relay();
    } else {
        LogPrint(BCLog::MNBUDGET,"CFinalizedBudget::SubmitVote : Error submitting vote - %s\n", strError);
    }
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast() :
        CFinalizedBudget()
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(const CFinalizedBudget& other) :
        CFinalizedBudget(other)
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(std::string strBudgetNameIn,
                                                     int nBlockStartIn,
                                                     std::vector<CTxBudgetPayment> vecBudgetPaymentsIn,
                                                     uint256 nFeeTXHashIn)
{
    strBudgetName = strBudgetNameIn;
    nBlockStart = nBlockStartIn;
    for (CTxBudgetPayment out : vecBudgetPaymentsIn)
        vecBudgetPayments.push_back(out);
    nFeeTXHash = nFeeTXHashIn;
}

void CFinalizedBudgetBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED, GetHash());
    RelayInv(inv);
}

CFinalizedBudgetVote::CFinalizedBudgetVote() :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(),
        nBudgetHash(),
        nTime(0)
{ }

CFinalizedBudgetVote::CFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn) :
        CSignedMessage(),
        fValid(true),
        fSynced(false),
        vin(vinIn),
        nBudgetHash(nBudgetHashIn)
{
    nTime = GetAdjustedTime();
}

void CFinalizedBudgetVote::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED_VOTE, GetHash());
    RelayInv(inv);
}

uint256 CFinalizedBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nBudgetHash;
    ss << nTime;
    return ss.GetHash();
}

std::string CFinalizedBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nBudgetHash.ToString() + std::to_string(nTime);
}

std::string CBudgetManager::ToString() const
{
    std::ostringstream info;

    info << "Proposals: " << (int)mapProposals.size() << ", Budgets: " << (int)mapFinalizedBudgets.size() << ", Seen Budgets: " << (int)mapSeenMasternodeBudgetProposals.size() << ", Seen Budget Votes: " << (int)mapSeenMasternodeBudgetVotes.size() << ", Seen Final Budgets: " << (int)mapSeenFinalizedBudgets.size() << ", Seen Final Budget Votes: " << (int)mapSeenFinalizedBudgetVotes.size();

    return info.str();
}
