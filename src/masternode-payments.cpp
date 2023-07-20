// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode-payments.h"

#include "chainparams.h"
#include "evo/deterministicmns.h"
#include "fs.h"
#include "budget/budgetmanager.h"
#include "masternodeman.h"
#include "netmessagemaker.h"
#include "tiertwo/netfulfilledman.h"
#include "spork.h"
#include "sync.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "util/system.h"
#include "utilmoneystr.h"
#include "validation.h"


/** Object for who's going to get paid on which blocks */
CMasternodePayments masternodePayments;

RecursiveMutex cs_vecPayments;
RecursiveMutex cs_mapMasternodeBlocks;
RecursiveMutex cs_mapMasternodePayeeVotes;

static const int MNPAYMENTS_DB_VERSION = 1;

//
// CMasternodePaymentDB
//

CMasternodePaymentDB::CMasternodePaymentDB()
{
    pathDB = GetDataDir() / "mnpayments.dat";
    strMagicMessage = "MasternodePayments";
}

bool CMasternodePaymentDB::Write(const CMasternodePayments& objToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << MNPAYMENTS_DB_VERSION;
    ssObj << strMagicMessage;                   // masternode cache file specific magic message
    ssObj << Params().MessageStart(); // network specific magic number
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

    LogPrint(BCLog::MASTERNODE,"Written info to mnpayments.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CMasternodePaymentDB::ReadResult CMasternodePaymentDB::Read(CMasternodePayments& objToLoad)
{
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
        filein.read((char*)vchData.data(), dataSize);
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

    int version;
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header
        ssObj >> version;
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid masternode payement cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        std::vector<unsigned char> pchMsgTmp(4);
        ssObj >> MakeSpan(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp.data(), Params().MessageStart(), pchMsgTmp.size()) != 0) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CMasternodePayments object
        ssObj >> objToLoad;
    } catch (const std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint(BCLog::MASTERNODE,"Loaded info from mnpayments.dat (dbversion=%d) %dms\n", version, GetTimeMillis() - nStart);
    LogPrint(BCLog::MASTERNODE,"  %s\n", objToLoad.ToString());

    return Ok;
}

uint256 CMasternodePaymentWinner::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << std::vector<unsigned char>(payee.begin(), payee.end());
    ss << nBlockHeight;
    ss << vinMasternode.prevout;
    return ss.GetHash();
}

std::string CMasternodePaymentWinner::GetStrMessage() const
{
    return vinMasternode.prevout.ToStringShort() + std::to_string(nBlockHeight) + HexStr(payee);
}

bool CMasternodePaymentWinner::IsValid(CNode* pnode, CValidationState& state, int chainHeight)
{
    int n = mnodeman.GetMasternodeRank(vinMasternode, nBlockHeight - 100);
    if (n < 1 || n > MNPAYMENTS_SIGNATURES_TOTAL) {
        return state.Error(strprintf("Masternode not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, n));
    }

    // Must be a P2PKH
    if (!payee.IsPayToPublicKeyHash()) {
        return state.Error("payee must be a P2PKH");
    }

    return true;
}

void CMasternodePaymentWinner::Relay()
{
    CInv inv(MSG_MASTERNODE_WINNER, GetHash());
    g_connman->RelayInv(inv);
}

void DumpMasternodePayments()
{
    int64_t nStart = GetTimeMillis();

    CMasternodePaymentDB paymentdb;
    LogPrint(BCLog::MASTERNODE,"Writing info to mnpayments.dat...\n");
    paymentdb.Write(masternodePayments);

    LogPrint(BCLog::MASTERNODE,"Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool IsBlockValueValid(int nHeight, CAmount& nExpectedValue, CAmount nMinted, CAmount& nBudgetAmt)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    if (!g_tiertwo_sync_state.IsSynced()) {
        //there is no budget data to use to check anything
        //super blocks will always be on these blocks, max 100 per budgeting
        if (nHeight % consensus.nBudgetCycleBlocks < 100) {
            if (Params().IsTestnet()) {
                return true;
            }
            nExpectedValue += g_budgetman.GetTotalBudget(nHeight);
        }
    } else {
        // we're synced and have data so check the budget schedule
        // if the superblock spork is enabled
        if (sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
            // add current payee amount to the expected block value
            if (g_budgetman.GetExpectedPayeeAmount(nHeight, nBudgetAmt)) {
                nExpectedValue += nBudgetAmt;
            }
        }
    }

    if (nMinted < 0 && consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V5_3)) {
        return false;
    }

    return nMinted <= nExpectedValue;
}

bool IsBlockPayeeValid(const CBlock& block, const CBlockIndex* pindexPrev)
{
    int nBlockHeight = pindexPrev->nHeight + 1;
    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;

    if (!g_tiertwo_sync_state.IsSynced()) { //there is no budget data to use to check anything -- find the longest chain
        LogPrint(BCLog::MASTERNODE, "Client not synced, skipping block payee checks\n");
        return true;
    }

    const bool fPayCoinstake = Params().GetConsensus().NetworkUpgradeActive(nBlockHeight, Consensus::UPGRADE_POS) &&
                               !Params().GetConsensus().NetworkUpgradeActive(nBlockHeight, Consensus::UPGRADE_V6_0);
    const CTransaction& txNew = *(fPayCoinstake ? block.vtx[1] : block.vtx[0]);

    //check if it's a budget block
    if (sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
        if (g_budgetman.IsBudgetPaymentBlock(nBlockHeight)) {
            transactionStatus = g_budgetman.IsTransactionValid(txNew, block.GetHash(), nBlockHeight);
            if (transactionStatus == TrxValidationStatus::Valid) {
                return true;
            }

            if (transactionStatus == TrxValidationStatus::InValid) {
                LogPrint(BCLog::MASTERNODE,"Invalid budget payment detected %s\n", txNew.ToString().c_str());
                if (sporkManager.IsSporkActive(SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT))
                    return false;

                LogPrint(BCLog::MASTERNODE,"Budget enforcement is disabled, accepting block\n");
            }
        }
    }

    // If we end here the transaction was either TrxValidationStatus::InValid and Budget enforcement is disabled, or
    // a double budget payment (status = TrxValidationStatus::DoublePayment) was detected, or no/not enough masternode
    // votes (status = TrxValidationStatus::VoteThreshold) for a finalized budget were found
    // In all cases a masternode will get the payment for this block

    //check for masternode payee
    if (masternodePayments.IsTransactionValid(txNew, pindexPrev))
        return true;
    LogPrint(BCLog::MASTERNODE,"Invalid mn payment detected %s\n", txNew.ToString().c_str());

    if (sporkManager.IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT))
        return false;
    LogPrint(BCLog::MASTERNODE,"Masternode payment enforcement is disabled, accepting block\n");
    return true;
}


void FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake)
{
    if (!sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) ||           // if superblocks are not enabled
            // ... or this is not a superblock
            !g_budgetman.FillBlockPayee(txCoinbase, txCoinstake, pindexPrev->nHeight + 1, fProofOfStake) ) {
        // ... or there's no budget with enough votes, then pay a masternode
        masternodePayments.FillBlockPayee(txCoinbase, txCoinstake, pindexPrev, fProofOfStake);
    }
}

std::string GetRequiredPaymentsString(int nBlockHeight)
{
    if (sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && g_budgetman.IsBudgetPaymentBlock(nBlockHeight)) {
        return g_budgetman.GetRequiredPaymentsString(nBlockHeight);
    } else {
        return masternodePayments.GetRequiredPaymentsString(nBlockHeight);
    }
}

bool CMasternodePayments::GetMasternodeTxOuts(const CBlockIndex* pindexPrev, std::vector<CTxOut>& voutMasternodePaymentsRet) const
{
    if (deterministicMNManager->LegacyMNObsolete(pindexPrev->nHeight + 1)) {
        CAmount masternodeReward = GetMasternodePayment(pindexPrev->nHeight + 1);
        auto dmnPayee = deterministicMNManager->GetListForBlock(pindexPrev).GetMNPayee();
        if (!dmnPayee) {
            return error("%s: Failed to get payees for block at height %d", __func__, pindexPrev->nHeight + 1);
        }
        CAmount operatorReward = 0;
        if (dmnPayee->nOperatorReward != 0 && !dmnPayee->pdmnState->scriptOperatorPayout.empty()) {
            operatorReward = (masternodeReward * dmnPayee->nOperatorReward) / 10000;
            masternodeReward -= operatorReward;
        }
        if (masternodeReward > 0) {
            voutMasternodePaymentsRet.emplace_back(masternodeReward, dmnPayee->pdmnState->scriptPayout);
        }
        if (operatorReward > 0) {
            voutMasternodePaymentsRet.emplace_back(operatorReward, dmnPayee->pdmnState->scriptOperatorPayout);
        }
        return true;
    }

    // Legacy payment logic. !TODO: remove when transition to DMN is complete
    return GetLegacyMasternodeTxOut(pindexPrev->nHeight + 1, voutMasternodePaymentsRet);
}

bool CMasternodePayments::GetLegacyMasternodeTxOut(int nHeight, std::vector<CTxOut>& voutMasternodePaymentsRet) const
{
    voutMasternodePaymentsRet.clear();

    CScript payee;
    if (!GetBlockPayee(nHeight, payee)) {
        //no masternode detected
        const uint256& hash = mnodeman.GetHashAtHeight(nHeight - 1);
        MasternodeRef winningNode = mnodeman.GetCurrentMasterNode(hash);
        if (winningNode) {
            payee = winningNode->GetPayeeScript();
        } else {
            LogPrint(BCLog::MASTERNODE,"CreateNewBlock: Failed to detect masternode to pay\n");
            return false;
        }
    }
    voutMasternodePaymentsRet.emplace_back(GetMasternodePayment(nHeight), payee);
    return true;
}

static void SubtractMnPaymentFromCoinstake(CMutableTransaction& txCoinstake, CAmount masternodePayment, int stakerOuts)
{
    assert (stakerOuts >= 2);
    //subtract mn payment from the stake reward
    if (stakerOuts == 2) {
        // Majority of cases; do it quick and move on
        txCoinstake.vout[1].nValue -= masternodePayment;
    } else {
        // special case, stake is split between (stakerOuts-1) outputs
        unsigned int outputs = stakerOuts-1;
        CAmount mnPaymentSplit = masternodePayment / outputs;
        CAmount mnPaymentRemainder = masternodePayment - (mnPaymentSplit * outputs);
        for (unsigned int j=1; j<=outputs; j++) {
            txCoinstake.vout[j].nValue -= mnPaymentSplit;
        }
        // in case it's not an even division, take the last bit of dust from the last one
        txCoinstake.vout[outputs].nValue -= mnPaymentRemainder;
    }
}

void CMasternodePayments::FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake) const
{
    std::vector<CTxOut> vecMnOuts;
    if (!GetMasternodeTxOuts(pindexPrev, vecMnOuts)) {
        return;
    }

    // Starting from hemis v6.0 masternode and budgets are paid in the coinbase tx
    const int nHeight = pindexPrev->nHeight + 1;
    bool fPayCoinstake = fProofOfStake && !Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V6_0);

    // if PoS block pays the coinbase, clear it first
    if (fProofOfStake && !fPayCoinstake) txCoinbase.vout.clear();

    const int initial_cstake_outs = txCoinstake.vout.size();

    CAmount masternodePayment{0};
    for (const CTxOut& mnOut: vecMnOuts) {
        // Add the mn payment to the coinstake/coinbase tx
        if (fPayCoinstake) {
            txCoinstake.vout.emplace_back(mnOut);
        } else {
            txCoinbase.vout.emplace_back(mnOut);
        }
        masternodePayment += mnOut.nValue;
        CTxDestination payeeDest;
        ExtractDestination(mnOut.scriptPubKey, payeeDest);
        LogPrint(BCLog::MASTERNODE,"Masternode payment of %s to %s\n", FormatMoney(mnOut.nValue), EncodeDestination(payeeDest));
    }

    // Subtract mn payment value from the block reward
    if (fProofOfStake) {
        SubtractMnPaymentFromCoinstake(txCoinstake, masternodePayment, initial_cstake_outs);
    } else {
        txCoinbase.vout[0].nValue = GetBlockValue(nHeight) - masternodePayment;
    }
}

bool CMasternodePayments::ProcessMessageMasternodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, CValidationState& state)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) return true;

    // Skip after legacy obsolete. !TODO: remove when transition to DMN is complete
    if (deterministicMNManager->LegacyMNObsolete()) {
        LogPrint(BCLog::MASTERNODE, "mnw - skip obsolete message %s\n", strCommand);
        return true;
    }

    if (strCommand == NetMsgType::GETMNWINNERS) {
        //Masternode Payments Request Sync
        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
            if (g_netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::GETMNWINNERS)) {
                LogPrint(BCLog::MASTERNODE, "%s: mnget - peer already asked me for the list\n", __func__);
                return state.DoS(20, false, REJECT_INVALID, "getmnwinners-request-already-fulfilled");
            }
        }

        g_netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::GETMNWINNERS);
        Sync(pfrom, nCountNeeded);
        LogPrint(BCLog::MASTERNODE, "mnget - Sent Masternode winners to peer %i\n", pfrom->GetId());
    } else if (strCommand == NetMsgType::MNWINNER) {
        //Masternode Payments Declare Winner
        CMasternodePaymentWinner winner;
        vRecv >> winner;
        if (pfrom->nVersion < ActiveProtocol()) return false;

        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(winner.GetHash(), MSG_MASTERNODE_WINNER);
        }

        ProcessMNWinner(winner, pfrom, state);
        return state.IsValid();
    }

    return true;
}

bool CMasternodePayments::ProcessMNWinner(CMasternodePaymentWinner& winner, CNode* pfrom, CValidationState& state)
{
    int nHeight = mnodeman.GetBestHeight();

    if (mapMasternodePayeeVotes.count(winner.GetHash())) {
        LogPrint(BCLog::MASTERNODE, "mnw - Already seen - %s bestHeight %d\n", winner.GetHash().ToString().c_str(), nHeight);
        g_tiertwo_sync_state.AddedMasternodeWinner(winner.GetHash());
        return false;
    }

    int nFirstBlock = nHeight - (mnodeman.CountEnabled() * 1.25);
    if (winner.nBlockHeight < nFirstBlock || winner.nBlockHeight > nHeight + 20) {
        LogPrint(BCLog::MASTERNODE, "mnw - winner out of range - FirstBlock %d Height %d bestHeight %d\n", nFirstBlock, winner.nBlockHeight, nHeight);
        return state.Error("block height out of range");
    }

    // reject old signature version
    if (winner.nMessVersion != MessageVersion::MESS_VER_HASH) {
        LogPrint(BCLog::MASTERNODE, "mnw - rejecting old message version %d\n", winner.nMessVersion);
        return state.Error("mnw old message version");
    }

    // See if the mnw signer exists, and whether it's a legacy or DMN masternode
    const CMasternode* pmn{nullptr};
    auto dmn = deterministicMNManager->GetListAtChainTip().GetMNByCollateral(winner.vinMasternode.prevout);
    if (dmn == nullptr) {
        // legacy masternode
        pmn = mnodeman.Find(winner.vinMasternode.prevout);
        if (pmn == nullptr) {
            // it could be a non-synced masternode. ask for the mnb
            LogPrint(BCLog::MASTERNODE, "mnw - unknown masternode %s\n", winner.vinMasternode.prevout.hash.ToString());
            // Only ask for missing items after the initial mnlist sync is complete
            if (pfrom && g_tiertwo_sync_state.IsMasternodeListSynced()) mnodeman.AskForMN(pfrom, winner.vinMasternode);
            return state.Error("Non-existent mnwinner voter");
        }
    }
    // either deterministic or legacy. not both
    assert((dmn && !pmn) || (!dmn && pmn));

    // See if the masternode is in the quorum (top-MNPAYMENTS_SIGNATURES_TOTAL)
    if (!winner.IsValid(pfrom, state, nHeight)) {
        // error cause set internally
        return false;
    }

    // See if this masternode has already voted for this block height
    if (!CanVote(winner.vinMasternode.prevout, winner.nBlockHeight)) {
        return state.Error("MN already voted");
    }

    // Check signature
    bool is_valid_sig = dmn ? winner.CheckSignature(dmn->pdmnState->pubKeyOperator.Get())
                            : winner.CheckSignature(pmn->pubKeyMasternode.GetID());

    if (!is_valid_sig) {
        LogPrint(BCLog::MASTERNODE, "%s : mnw - invalid signature for %s masternode: %s\n",
                __func__, (dmn ? "deterministic" : "legacy"), winner.vinMasternode.prevout.hash.ToString());
        return state.DoS(20, false, REJECT_INVALID, "invalid voter mnwinner signature");
    }

    // Record vote
    RecordWinnerVote(winner.vinMasternode.prevout, winner.nBlockHeight);

    // Add winner
    AddWinningMasternode(winner);

    // Relay only if we are synchronized.
    // Makes no sense to relay MNWinners to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) winner.Relay();
    g_tiertwo_sync_state.AddedMasternodeWinner(winner.GetHash());

    // valid
    return true;
}

bool CMasternodePayments::GetBlockPayee(int nBlockHeight, CScript& payee) const
{
    const auto it = mapMasternodeBlocks.find(nBlockHeight);
    if (it != mapMasternodeBlocks.end()) {
        return it->second.GetPayee(payee);
    }

    return false;
}

// Is this masternode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 winners
bool CMasternodePayments::IsScheduled(const CMasternode& mn, int nNotBlockHeight)
{
    LOCK(cs_mapMasternodeBlocks);

    int nHeight = mnodeman.GetBestHeight();

    const CScript& mnpayee = mn.GetPayeeScript();
    CScript payee;
    for (int64_t h = nHeight; h <= nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapMasternodeBlocks.count(h)) {
            if (mapMasternodeBlocks[h].GetPayee(payee)) {
                if (mnpayee == payee) {
                    return true;
                }
            }
        }
    }

    return false;
}

void CMasternodePayments::AddWinningMasternode(CMasternodePaymentWinner& winnerIn)
{
    {
        LOCK2(cs_mapMasternodePayeeVotes, cs_mapMasternodeBlocks);

        mapMasternodePayeeVotes[winnerIn.GetHash()] = winnerIn;

        if (!mapMasternodeBlocks.count(winnerIn.nBlockHeight)) {
            CMasternodeBlockPayees blockPayees(winnerIn.nBlockHeight);
            mapMasternodeBlocks[winnerIn.nBlockHeight] = blockPayees;
        }
    }

    CTxDestination addr;
    ExtractDestination(winnerIn.payee, addr);
    LogPrint(BCLog::MASTERNODE, "mnw - Adding winner %s for block %d\n", EncodeDestination(addr), winnerIn.nBlockHeight);
    mapMasternodeBlocks[winnerIn.nBlockHeight].AddPayee(winnerIn.payee, 1);
}

bool CMasternodeBlockPayees::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_vecPayments);

    //require at least 6 signatures
    int nMaxSignatures = 0;
    for (CMasternodePayee& payee : vecPayments)
        if (payee.nVotes >= nMaxSignatures && payee.nVotes >= MNPAYMENTS_SIGNATURES_REQUIRED)
            nMaxSignatures = payee.nVotes;

    // if we don't have at least 6 signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    std::string strPayeesPossible = "";
    CAmount requiredMasternodePayment = GetMasternodePayment(nBlockHeight);

    for (CMasternodePayee& payee : vecPayments) {
        bool found = false;
        for (CTxOut out : txNew.vout) {
            if (payee.scriptPubKey == out.scriptPubKey) {
                if(out.nValue == requiredMasternodePayment)
                    found = true;
                else
                    LogPrintf("%s : Masternode payment value (%s) different from required value (%s).\n",
                            __func__, FormatMoney(out.nValue).c_str(), FormatMoney(requiredMasternodePayment).c_str());
            }
        }

        if (payee.nVotes >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            if (found) return true;

            CTxDestination address1;
            ExtractDestination(payee.scriptPubKey, address1);

            if (strPayeesPossible != "")
                strPayeesPossible += ",";

            strPayeesPossible += EncodeDestination(address1);
        }
    }

    LogPrint(BCLog::MASTERNODE,"CMasternodePayments::IsTransactionValid - Missing required payment of %s to %s\n", FormatMoney(requiredMasternodePayment).c_str(), strPayeesPossible.c_str());
    return false;
}

std::string CMasternodeBlockPayees::GetRequiredPaymentsString()
{
    LOCK(cs_vecPayments);

    std::string ret = "";

    for (CMasternodePayee& payee : vecPayments) {
        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);
        if (ret != "") {
            ret += ", ";
        }
        ret += EncodeDestination(address1) + ":" + std::to_string(payee.nVotes);
    }

    return ret.empty() ? "Unknown" : ret;
}

std::string CMasternodePayments::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CMasternodePayments::IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev)
{
    const int nBlockHeight = pindexPrev->nHeight + 1;
    if (deterministicMNManager->LegacyMNObsolete(nBlockHeight)) {
        std::vector<CTxOut> vecMnOuts;
        if (!GetMasternodeTxOuts(pindexPrev, vecMnOuts)) {
            // No masternode scheduled to be paid.
            return true;
        }

        for (const CTxOut& o : vecMnOuts) {
            if (std::find(txNew.vout.begin(), txNew.vout.end(), o) == txNew.vout.end()) {
                CTxDestination mnDest;
                const std::string& payee = ExtractDestination(o.scriptPubKey, mnDest) ? EncodeDestination(mnDest)
                                                                                      : HexStr(o.scriptPubKey);
                LogPrint(BCLog::MASTERNODE, "%s: Failed to find expected payee %s in block at height %d (tx %s)",
                                            __func__, payee, pindexPrev->nHeight + 1, txNew.GetHash().ToString());
                return false;
            }
        }
        // all the expected payees have been found in txNew outputs
        return true;
    }

    // Legacy payment logic. !TODO: remove when transition to DMN is complete
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].IsTransactionValid(txNew, nBlockHeight);
    }

    return true;
}

void CMasternodePayments::CleanPaymentList(int mnCount, int nHeight)
{
    LOCK2(cs_mapMasternodePayeeVotes, cs_mapMasternodeBlocks);

    //keep up to five cycles for historical sake
    int nLimit = std::max(int(mnCount * 1.25), 1000);

    std::map<uint256, CMasternodePaymentWinner>::iterator it = mapMasternodePayeeVotes.begin();
    while (it != mapMasternodePayeeVotes.end()) {
        CMasternodePaymentWinner winner = (*it).second;

        if (nHeight - winner.nBlockHeight > nLimit) {
            LogPrint(BCLog::MASTERNODE, "CMasternodePayments::CleanPaymentList - Removing old Masternode payment - block %d\n", winner.nBlockHeight);
            g_tiertwo_sync_state.EraseSeenMNW((*it).first);
            mapMasternodePayeeVotes.erase(it++);
            mapMasternodeBlocks.erase(winner.nBlockHeight);
        } else {
            ++it;
        }
    }
}

void CMasternodePayments::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    if (g_tiertwo_sync_state.GetSyncPhase() > MASTERNODE_SYNC_LIST) {
        ProcessBlock(pindexNew->nHeight + 10);
    }
}

void CMasternodePayments::ProcessBlock(int nBlockHeight)
{
    // No more mnw messages after transition to DMN
    if (deterministicMNManager->LegacyMNObsolete(nBlockHeight)) {
        return;
    }
    if (!fMasterNode) return;

    // Get the active masternode (operator) key
    CTxIn mnVin;
    Optional<CKey> mnKey{nullopt};
    CBLSSecretKey blsKey;
    if (!GetActiveMasternodeKeys(mnVin, mnKey, blsKey)) {
        return;
    }

    //reference node - hybrid mode
    int n = mnodeman.GetMasternodeRank(mnVin, nBlockHeight - 100);

    if (n == -1) {
        LogPrintf("%s: ERROR: active masternode is not registered yet\n", __func__);
        return;
    }

    if (n > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrintf("%s: active masternode not in the top %d (%d)\n", __func__, MNPAYMENTS_SIGNATURES_TOTAL, n);
        return;
    }

    if (nBlockHeight <= nLastBlockHeight) return;

    if (g_budgetman.IsBudgetPaymentBlock(nBlockHeight)) {
        //is budget payment block -- handled by the budgeting software
        return;
    }

    // check winner height
    if (nBlockHeight - 100 > mnodeman.GetBestHeight() + 1) {
        LogPrintf("%s: mnw - invalid height %d > %d", __func__, nBlockHeight - 100, mnodeman.GetBestHeight() + 1);
        return;
    }

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    MasternodeRef pmn = mnodeman.GetNextMasternodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == nullptr) {
        LogPrintf("%s: Failed to find masternode to pay\n", __func__);
        return;
    }

    CMasternodePaymentWinner newWinner(mnVin, nBlockHeight);
    newWinner.AddPayee(pmn->GetPayeeScript());
    if (mnKey != nullopt) {
        // Legacy MN
        if (!newWinner.Sign(*mnKey, mnKey->GetPubKey().GetID())) {
            LogPrintf("%s: Failed to sign masternode winner\n", __func__);
            return;
        }
    } else {
        // DMN
        if (!newWinner.Sign(blsKey)) {
            LogPrintf("%s: Failed to sign masternode winner with DMN\n", __func__);
            return;
        }
    }

    AddWinningMasternode(newWinner);
    newWinner.Relay();
    LogPrintf("%s: Relayed winner %s\n", __func__, newWinner.GetHash().ToString());
    nLastBlockHeight = nBlockHeight;
}

void CMasternodePayments::Sync(CNode* node, int nCountNeeded)
{
    LOCK(cs_mapMasternodePayeeVotes);

    int nHeight = mnodeman.GetBestHeight();
    int nCount = (mnodeman.CountEnabled() * 1.25);
    if (nCountNeeded > nCount) nCountNeeded = nCount;

    int nInvCount = 0;
    std::map<uint256, CMasternodePaymentWinner>::iterator it = mapMasternodePayeeVotes.begin();
    while (it != mapMasternodePayeeVotes.end()) {
        CMasternodePaymentWinner winner = (*it).second;
        if (winner.nBlockHeight >= nHeight - nCountNeeded && winner.nBlockHeight <= nHeight + 20) {
            node->PushInventory(CInv(MSG_MASTERNODE_WINNER, winner.GetHash()));
            nInvCount++;
        }
        ++it;
    }
    g_connman->PushMessage(node, CNetMsgMaker(node->GetSendVersion()).Make(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_MNW, nInvCount));
}

std::string CMasternodePayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapMasternodePayeeVotes.size() << ", Blocks: " << (int)mapMasternodeBlocks.size();

    return info.str();
}

bool CMasternodePayments::CanVote(const COutPoint& outMasternode, int nBlockHeight) const
{
    LOCK(cs_mapMasternodePayeeVotes);
    const auto it = mapMasternodesLastVote.find(outMasternode);
    return it == mapMasternodesLastVote.end() || it->second != nBlockHeight;
}

void CMasternodePayments::RecordWinnerVote(const COutPoint& outMasternode, int nBlockHeight)
{
    LOCK(cs_mapMasternodePayeeVotes);
    mapMasternodesLastVote[outMasternode] = nBlockHeight;
}

bool IsCoinbaseValueValid(const CTransactionRef& tx, CAmount nBudgetAmt, CValidationState& _state)
{
    assert(tx->IsCoinBase());
    if (g_tiertwo_sync_state.IsSynced()) {
        const CAmount nCBaseOutAmt = tx->GetValueOut();
        if (nBudgetAmt > 0) {
            // Superblock
            if (nCBaseOutAmt != nBudgetAmt) {
                const std::string strError = strprintf("%s: invalid coinbase payment for budget (%s vs expected=%s)",
                                                       __func__, FormatMoney(nCBaseOutAmt), FormatMoney(nBudgetAmt));
                return _state.DoS(100, error(strError.c_str()), REJECT_INVALID, "bad-superblock-cb-amt");
            }
            return true;
        } else {
            // regular block
            int nHeight = mnodeman.GetBestHeight();
            CAmount nMnAmt = GetMasternodePayment(nHeight);
            // if enforcement is disabled, there could be no masternode payment
            bool sporkEnforced = sporkManager.IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT);
            const std::string strError = strprintf("%s: invalid coinbase payment for masternode (%s vs expected=%s)",
                                                   __func__, FormatMoney(nCBaseOutAmt), FormatMoney(nMnAmt));
            if (sporkEnforced && nCBaseOutAmt != nMnAmt) {
                return _state.DoS(100, error(strError.c_str()), REJECT_INVALID, "bad-cb-amt");
            }
            if (!sporkEnforced && nCBaseOutAmt > nMnAmt) {
                return _state.DoS(100, error(strError.c_str()), REJECT_INVALID, "bad-cb-amt-spork8-disabled");
            }
            return true;
        }
    }
    return true;
}
