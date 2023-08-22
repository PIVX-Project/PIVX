// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamemaster-payments.h"

#include "chainparams.h"
#include "evo/deterministicgms.h"
#include "fs.h"
#include "budget/budgetmanager.h"
#include "gamemasterman.h"
#include "netmessagemaker.h"
#include "tiertwo/netfulfilledman.h"
#include "spork.h"
#include "sync.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "util/system.h"
#include "utilmoneystr.h"
#include "validation.h"


/** Object for who's going to get paid on which blocks */
CGamemasterPayments gamemasterPayments;

RecursiveMutex cs_vecPayments;
RecursiveMutex cs_mapGamemasterBlocks;
RecursiveMutex cs_mapGamemasterPayeeVotes;

static const int GMPAYMENTS_DB_VERSION = 1;

//
// CGamemasterPaymentDB
//

CGamemasterPaymentDB::CGamemasterPaymentDB()
{
    pathDB = GetDataDir() / "gmpayments.dat";
    strMagicMessage = "GamemasterPayments";
}

bool CGamemasterPaymentDB::Write(const CGamemasterPayments& objToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << GMPAYMENTS_DB_VERSION;
    ssObj << strMagicMessage;                   // gamemaster cache file specific magic message
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

    LogPrint(BCLog::GAMEMASTER,"Written info to gmpayments.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CGamemasterPaymentDB::ReadResult CGamemasterPaymentDB::Read(CGamemasterPayments& objToLoad)
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
            error("%s : Invalid gamemaster payement cache magic message", __func__);
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

        // de-serialize data into CGamemasterPayments object
        ssObj >> objToLoad;
    } catch (const std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint(BCLog::GAMEMASTER,"Loaded info from gmpayments.dat (dbversion=%d) %dms\n", version, GetTimeMillis() - nStart);
    LogPrint(BCLog::GAMEMASTER,"  %s\n", objToLoad.ToString());

    return Ok;
}

uint256 CGamemasterPaymentWinner::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << std::vector<unsigned char>(payee.begin(), payee.end());
    ss << nBlockHeight;
    ss << vinGamemaster.prevout;
    return ss.GetHash();
}

std::string CGamemasterPaymentWinner::GetStrMessage() const
{
    return vinGamemaster.prevout.ToStringShort() + std::to_string(nBlockHeight) + HexStr(payee);
}

bool CGamemasterPaymentWinner::IsValid(CNode* pnode, CValidationState& state, int chainHeight)
{
    int n = gamemasterman.GetGamemasterRank(vinGamemaster, nBlockHeight - 100);
    if (n < 1 || n > GMPAYMENTS_SIGNATURES_TOTAL) {
        return state.Error(strprintf("Gamemaster not in the top %d (%d)", GMPAYMENTS_SIGNATURES_TOTAL, n));
    }

    // Must be a P2PKH
    if (!payee.IsPayToPublicKeyHash()) {
        return state.Error("payee must be a P2PKH");
    }

    return true;
}

void CGamemasterPaymentWinner::Relay()
{
    CInv inv(MSG_GAMEMASTER_WINNER, GetHash());
    g_connman->RelayInv(inv);
}

void DumpGamemasterPayments()
{
    int64_t nStart = GetTimeMillis();

    CGamemasterPaymentDB paymentdb;
    LogPrint(BCLog::GAMEMASTER,"Writing info to gmpayments.dat...\n");
    paymentdb.Write(gamemasterPayments);

    LogPrint(BCLog::GAMEMASTER,"Budget dump finished  %dms\n", GetTimeMillis() - nStart);
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
        LogPrint(BCLog::GAMEMASTER, "Client not synced, skipping block payee checks\n");
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
                LogPrint(BCLog::GAMEMASTER,"Invalid budget payment detected %s\n", txNew.ToString().c_str());
                if (sporkManager.IsSporkActive(SPORK_9_GAMEMASTER_BUDGET_ENFORCEMENT))
                    return false;

                LogPrint(BCLog::GAMEMASTER,"Budget enforcement is disabled, accepting block\n");
            }
        }
    }

    // If we end here the transaction was either TrxValidationStatus::InValid and Budget enforcement is disabled, or
    // a double budget payment (status = TrxValidationStatus::DoublePayment) was detected, or no/not enough gamemaster
    // votes (status = TrxValidationStatus::VoteThreshold) for a finalized budget were found
    // In all cases a gamemaster will get the payment for this block

    //check for gamemaster payee
    if (gamemasterPayments.IsTransactionValid(txNew, pindexPrev))
        return true;
    LogPrint(BCLog::GAMEMASTER,"Invalid gm payment detected %s\n", txNew.ToString().c_str());

    if (sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT))
        return false;
    LogPrint(BCLog::GAMEMASTER,"Gamemaster payment enforcement is disabled, accepting block\n");
    return true;
}


void FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake)
{
    if (!sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) ||           // if superblocks are not enabled
            // ... or this is not a superblock
            !g_budgetman.FillBlockPayee(txCoinbase, txCoinstake, pindexPrev->nHeight + 1, fProofOfStake) ) {
        // ... or there's no budget with enough votes, then pay a gamemaster
        gamemasterPayments.FillBlockPayee(txCoinbase, txCoinstake, pindexPrev, fProofOfStake);
    }
}

std::string GetRequiredPaymentsString(int nBlockHeight)
{
    if (sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && g_budgetman.IsBudgetPaymentBlock(nBlockHeight)) {
        return g_budgetman.GetRequiredPaymentsString(nBlockHeight);
    } else {
        return gamemasterPayments.GetRequiredPaymentsString(nBlockHeight);
    }
}

bool CGamemasterPayments::GetGamemasterTxOuts(const CBlockIndex* pindexPrev, std::vector<CTxOut>& voutGamemasterPaymentsRet) const
{
    if (deterministicGMManager->LegacyGMObsolete(pindexPrev->nHeight + 1)) {
        CAmount gamemasterReward = GetGamemasterPayment(pindexPrev->nHeight + 1);
        auto dgmPayee = deterministicGMManager->GetListForBlock(pindexPrev).GetGMPayee();
        if (!dgmPayee) {
            return error("%s: Failed to get payees for block at height %d", __func__, pindexPrev->nHeight + 1);
        }
        CAmount operatorReward = 0;
        if (dgmPayee->nOperatorReward != 0 && !dgmPayee->pdgmState->scriptOperatorPayout.empty()) {
            operatorReward = (gamemasterReward * dgmPayee->nOperatorReward) / 10000;
            gamemasterReward -= operatorReward;
        }
        if (gamemasterReward > 0) {
            voutGamemasterPaymentsRet.emplace_back(gamemasterReward, dgmPayee->pdgmState->scriptPayout);
        }
        if (operatorReward > 0) {
            voutGamemasterPaymentsRet.emplace_back(operatorReward, dgmPayee->pdgmState->scriptOperatorPayout);
        }
        return true;
    }

    // Legacy payment logic. !TODO: remove when transition to DGM is complete
    return GetLegacyGamemasterTxOut(pindexPrev->nHeight + 1, voutGamemasterPaymentsRet);
}

bool CGamemasterPayments::GetLegacyGamemasterTxOut(int nHeight, std::vector<CTxOut>& voutGamemasterPaymentsRet) const
{
    voutGamemasterPaymentsRet.clear();

    CScript payee;
    if (!GetBlockPayee(nHeight, payee)) {
        //no gamemaster detected
        const uint256& hash = gamemasterman.GetHashAtHeight(nHeight - 1);
        GamemasterRef winningNode = gamemasterman.GetCurrentMasterNode(hash);
        if (winningNode) {
            payee = winningNode->GetPayeeScript();
        } else {
            LogPrint(BCLog::GAMEMASTER,"CreateNewBlock: Failed to detect gamemaster to pay\n");
            return false;
        }
    }
    voutGamemasterPaymentsRet.emplace_back(GetGamemasterPayment(nHeight), payee);
    return true;
}

static void SubtractGmPaymentFromCoinstake(CMutableTransaction& txCoinstake, CAmount gamemasterPayment, int stakerOuts)
{
    assert (stakerOuts >= 2);
    //subtract gm payment from the stake reward
    if (stakerOuts == 2) {
        // Majority of cases; do it quick and move on
        txCoinstake.vout[1].nValue -= gamemasterPayment;
    } else {
        // special case, stake is split between (stakerOuts-1) outputs
        unsigned int outputs = stakerOuts-1;
        CAmount gmPaymentSplit = gamemasterPayment / outputs;
        CAmount gmPaymentRemainder = gamemasterPayment - (gmPaymentSplit * outputs);
        for (unsigned int j=1; j<=outputs; j++) {
            txCoinstake.vout[j].nValue -= gmPaymentSplit;
        }
        // in case it's not an even division, take the last bit of dust from the last one
        txCoinstake.vout[outputs].nValue -= gmPaymentRemainder;
    }
}

void CGamemasterPayments::FillBlockPayee(CMutableTransaction& txCoinbase, CMutableTransaction& txCoinstake, const CBlockIndex* pindexPrev, bool fProofOfStake) const
{
    std::vector<CTxOut> vecGmOuts;
    if (!GetGamemasterTxOuts(pindexPrev, vecGmOuts)) {
        return;
    }

    // Starting from hemis v6.0 gamemaster and budgets are paid in the coinbase tx
    const int nHeight = pindexPrev->nHeight + 1;
    bool fPayCoinstake = fProofOfStake && !Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V6_0);

    // if PoS block pays the coinbase, clear it first
    if (fProofOfStake && !fPayCoinstake) txCoinbase.vout.clear();

    const int initial_cstake_outs = txCoinstake.vout.size();

    CAmount gamemasterPayment{0};
    for (const CTxOut& gmOut: vecGmOuts) {
        // Add the gm payment to the coinstake/coinbase tx
        if (fPayCoinstake) {
            txCoinstake.vout.emplace_back(gmOut);
        } else {
            txCoinbase.vout.emplace_back(gmOut);
        }
        gamemasterPayment += gmOut.nValue;
        CTxDestination payeeDest;
        ExtractDestination(gmOut.scriptPubKey, payeeDest);
        LogPrint(BCLog::GAMEMASTER,"Gamemaster payment of %s to %s\n", FormatMoney(gmOut.nValue), EncodeDestination(payeeDest));
    }

    // Subtract gm payment value from the block reward
    if (fProofOfStake) {
        SubtractGmPaymentFromCoinstake(txCoinstake, gamemasterPayment, initial_cstake_outs);
    } else {
        txCoinbase.vout[0].nValue = GetBlockValue(nHeight) - gamemasterPayment;
    }
}

bool CGamemasterPayments::ProcessMessageGamemasterPayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, CValidationState& state)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) return true;

    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        LogPrint(BCLog::GAMEMASTER, "gmw - skip obsolete message %s\n", strCommand);
        return true;
    }

    if (strCommand == NetMsgType::GETGMWINNERS) {
        //Gamemaster Payments Request Sync
        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
            if (g_netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::GETGMWINNERS)) {
                LogPrint(BCLog::GAMEMASTER, "%s: gmget - peer already asked me for the list\n", __func__);
                return state.DoS(20, false, REJECT_INVALID, "getgmwinners-request-already-fulfilled");
            }
        }

        g_netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::GETGMWINNERS);
        Sync(pfrom, nCountNeeded);
        LogPrint(BCLog::GAMEMASTER, "gmget - Sent Gamemaster winners to peer %i\n", pfrom->GetId());
    } else if (strCommand == NetMsgType::GMWINNER) {
        //Gamemaster Payments Declare Winner
        CGamemasterPaymentWinner winner;
        vRecv >> winner;
        if (pfrom->nVersion < ActiveProtocol()) return false;

        {
            // Clear inv request
            LOCK(cs_main);
            g_connman->RemoveAskFor(winner.GetHash(), MSG_GAMEMASTER_WINNER);
        }

        ProcessGMWinner(winner, pfrom, state);
        return state.IsValid();
    }

    return true;
}

bool CGamemasterPayments::ProcessGMWinner(CGamemasterPaymentWinner& winner, CNode* pfrom, CValidationState& state)
{
    int nHeight = gamemasterman.GetBestHeight();

    if (mapGamemasterPayeeVotes.count(winner.GetHash())) {
        LogPrint(BCLog::GAMEMASTER, "gmw - Already seen - %s bestHeight %d\n", winner.GetHash().ToString().c_str(), nHeight);
        g_tiertwo_sync_state.AddedGamemasterWinner(winner.GetHash());
        return false;
    }

    int nFirstBlock = nHeight - (gamemasterman.CountEnabled() * 1.25);
    if (winner.nBlockHeight < nFirstBlock || winner.nBlockHeight > nHeight + 20) {
        LogPrint(BCLog::GAMEMASTER, "gmw - winner out of range - FirstBlock %d Height %d bestHeight %d\n", nFirstBlock, winner.nBlockHeight, nHeight);
        return state.Error("block height out of range");
    }

    // reject old signature version
    if (winner.nMessVersion != MessageVersion::MESS_VER_HASH) {
        LogPrint(BCLog::GAMEMASTER, "gmw - rejecting old message version %d\n", winner.nMessVersion);
        return state.Error("gmw old message version");
    }

    // See if the gmw signer exists, and whether it's a legacy or DGM gamemaster
    const CGamemaster* pgm{nullptr};
    auto dgm = deterministicGMManager->GetListAtChainTip().GetGMByCollateral(winner.vinGamemaster.prevout);
    if (dgm == nullptr) {
        // legacy gamemaster
        pgm = gamemasterman.Find(winner.vinGamemaster.prevout);
        if (pgm == nullptr) {
            // it could be a non-synced gamemaster. ask for the gmb
            LogPrint(BCLog::GAMEMASTER, "gmw - unknown gamemaster %s\n", winner.vinGamemaster.prevout.hash.ToString());
            // Only ask for missing items after the initial gmlist sync is complete
            if (pfrom && g_tiertwo_sync_state.IsGamemasterListSynced()) gamemasterman.AskForGM(pfrom, winner.vinGamemaster);
            return state.Error("Non-existent gmwinner voter");
        }
    }
    // either deterministic or legacy. not both
    assert((dgm && !pgm) || (!dgm && pgm));

    // See if the gamemaster is in the quorum (top-GMPAYMENTS_SIGNATURES_TOTAL)
    if (!winner.IsValid(pfrom, state, nHeight)) {
        // error cause set internally
        return false;
    }

    // See if this gamemaster has already voted for this block height
    if (!CanVote(winner.vinGamemaster.prevout, winner.nBlockHeight)) {
        return state.Error("GM already voted");
    }

    // Check signature
    bool is_valid_sig = dgm ? winner.CheckSignature(dgm->pdgmState->pubKeyOperator.Get())
                            : winner.CheckSignature(pgm->pubKeyGamemaster.GetID());

    if (!is_valid_sig) {
        LogPrint(BCLog::GAMEMASTER, "%s : gmw - invalid signature for %s gamemaster: %s\n",
                __func__, (dgm ? "deterministic" : "legacy"), winner.vinGamemaster.prevout.hash.ToString());
        return state.DoS(20, false, REJECT_INVALID, "invalid voter gmwinner signature");
    }

    // Record vote
    RecordWinnerVote(winner.vinGamemaster.prevout, winner.nBlockHeight);

    // Add winner
    AddWinningGamemaster(winner);

    // Relay only if we are synchronized.
    // Makes no sense to relay GMWinners to the peers from where we are syncing them.
    if (g_tiertwo_sync_state.IsSynced()) winner.Relay();
    g_tiertwo_sync_state.AddedGamemasterWinner(winner.GetHash());

    // valid
    return true;
}

bool CGamemasterPayments::GetBlockPayee(int nBlockHeight, CScript& payee) const
{
    const auto it = mapGamemasterBlocks.find(nBlockHeight);
    if (it != mapGamemasterBlocks.end()) {
        return it->second.GetPayee(payee);
    }

    return false;
}

// Is this gamemaster scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 winners
bool CGamemasterPayments::IsScheduled(const CGamemaster& gm, int nNotBlockHeight)
{
    LOCK(cs_mapGamemasterBlocks);

    int nHeight = gamemasterman.GetBestHeight();

    const CScript& gmpayee = gm.GetPayeeScript();
    CScript payee;
    for (int64_t h = nHeight; h <= nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapGamemasterBlocks.count(h)) {
            if (mapGamemasterBlocks[h].GetPayee(payee)) {
                if (gmpayee == payee) {
                    return true;
                }
            }
        }
    }

    return false;
}

void CGamemasterPayments::AddWinningGamemaster(CGamemasterPaymentWinner& winnerIn)
{
    {
        LOCK2(cs_mapGamemasterPayeeVotes, cs_mapGamemasterBlocks);

        mapGamemasterPayeeVotes[winnerIn.GetHash()] = winnerIn;

        if (!mapGamemasterBlocks.count(winnerIn.nBlockHeight)) {
            CGamemasterBlockPayees blockPayees(winnerIn.nBlockHeight);
            mapGamemasterBlocks[winnerIn.nBlockHeight] = blockPayees;
        }
    }

    CTxDestination addr;
    ExtractDestination(winnerIn.payee, addr);
    LogPrint(BCLog::GAMEMASTER, "gmw - Adding winner %s for block %d\n", EncodeDestination(addr), winnerIn.nBlockHeight);
    mapGamemasterBlocks[winnerIn.nBlockHeight].AddPayee(winnerIn.payee, 1);
}

bool CGamemasterBlockPayees::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_vecPayments);

    //require at least 6 signatures
    int nMaxSignatures = 0;
    for (CGamemasterPayee& payee : vecPayments)
        if (payee.nVotes >= nMaxSignatures && payee.nVotes >= GMPAYMENTS_SIGNATURES_REQUIRED)
            nMaxSignatures = payee.nVotes;

    // if we don't have at least 6 signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < GMPAYMENTS_SIGNATURES_REQUIRED) return true;

    std::string strPayeesPossible = "";
    CAmount requiredGamemasterPayment = GetGamemasterPayment(nBlockHeight);

    for (CGamemasterPayee& payee : vecPayments) {
        bool found = false;
        for (CTxOut out : txNew.vout) {
            if (payee.scriptPubKey == out.scriptPubKey) {
                if(out.nValue == requiredGamemasterPayment)
                    found = true;
                else
                    LogPrintf("%s : Gamemaster payment value (%s) different from required value (%s).\n",
                            __func__, FormatMoney(out.nValue).c_str(), FormatMoney(requiredGamemasterPayment).c_str());
            }
        }

        if (payee.nVotes >= GMPAYMENTS_SIGNATURES_REQUIRED) {
            if (found) return true;

            CTxDestination address1;
            ExtractDestination(payee.scriptPubKey, address1);

            if (strPayeesPossible != "")
                strPayeesPossible += ",";

            strPayeesPossible += EncodeDestination(address1);
        }
    }

    LogPrint(BCLog::GAMEMASTER,"CGamemasterPayments::IsTransactionValid - Missing required payment of %s to %s\n", FormatMoney(requiredGamemasterPayment).c_str(), strPayeesPossible.c_str());
    return false;
}

std::string CGamemasterBlockPayees::GetRequiredPaymentsString()
{
    LOCK(cs_vecPayments);

    std::string ret = "";

    for (CGamemasterPayee& payee : vecPayments) {
        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);
        if (ret != "") {
            ret += ", ";
        }
        ret += EncodeDestination(address1) + ":" + std::to_string(payee.nVotes);
    }

    return ret.empty() ? "Unknown" : ret;
}

std::string CGamemasterPayments::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapGamemasterBlocks);

    if (mapGamemasterBlocks.count(nBlockHeight)) {
        return mapGamemasterBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CGamemasterPayments::IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev)
{
    const int nBlockHeight = pindexPrev->nHeight + 1;
    if (deterministicGMManager->LegacyGMObsolete(nBlockHeight)) {
        std::vector<CTxOut> vecGmOuts;
        if (!GetGamemasterTxOuts(pindexPrev, vecGmOuts)) {
            // No gamemaster scheduled to be paid.
            return true;
        }

        for (const CTxOut& o : vecGmOuts) {
            if (std::find(txNew.vout.begin(), txNew.vout.end(), o) == txNew.vout.end()) {
                CTxDestination gmDest;
                const std::string& payee = ExtractDestination(o.scriptPubKey, gmDest) ? EncodeDestination(gmDest)
                                                                                      : HexStr(o.scriptPubKey);
                LogPrint(BCLog::GAMEMASTER, "%s: Failed to find expected payee %s in block at height %d (tx %s)",
                                            __func__, payee, pindexPrev->nHeight + 1, txNew.GetHash().ToString());
                return false;
            }
        }
        // all the expected payees have been found in txNew outputs
        return true;
    }

    // Legacy payment logic. !TODO: remove when transition to DGM is complete
    LOCK(cs_mapGamemasterBlocks);

    if (mapGamemasterBlocks.count(nBlockHeight)) {
        return mapGamemasterBlocks[nBlockHeight].IsTransactionValid(txNew, nBlockHeight);
    }

    return true;
}

void CGamemasterPayments::CleanPaymentList(int gmCount, int nHeight)
{
    LOCK2(cs_mapGamemasterPayeeVotes, cs_mapGamemasterBlocks);

    //keep up to five cycles for historical sake
    int nLimit = std::max(int(gmCount * 1.25), 1000);

    std::map<uint256, CGamemasterPaymentWinner>::iterator it = mapGamemasterPayeeVotes.begin();
    while (it != mapGamemasterPayeeVotes.end()) {
        CGamemasterPaymentWinner winner = (*it).second;

        if (nHeight - winner.nBlockHeight > nLimit) {
            LogPrint(BCLog::GAMEMASTER, "CGamemasterPayments::CleanPaymentList - Removing old Gamemaster payment - block %d\n", winner.nBlockHeight);
            g_tiertwo_sync_state.EraseSeenGMW((*it).first);
            mapGamemasterPayeeVotes.erase(it++);
            mapGamemasterBlocks.erase(winner.nBlockHeight);
        } else {
            ++it;
        }
    }
}

void CGamemasterPayments::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    if (g_tiertwo_sync_state.GetSyncPhase() > GAMEMASTER_SYNC_LIST) {
        ProcessBlock(pindexNew->nHeight + 10);
    }
}

void CGamemasterPayments::ProcessBlock(int nBlockHeight)
{
    // No more gmw messages after transition to DGM
    if (deterministicGMManager->LegacyGMObsolete(nBlockHeight)) {
        return;
    }
    if (!fMasterNode) return;

    // Get the active gamemaster (operator) key
    CTxIn gmVin;
    Optional<CKey> gmKey{nullopt};
    CBLSSecretKey blsKey;
    if (!GetActiveGamemasterKeys(gmVin, gmKey, blsKey)) {
        return;
    }

    //reference node - hybrid mode
    int n = gamemasterman.GetGamemasterRank(gmVin, nBlockHeight - 100);

    if (n == -1) {
        LogPrintf("%s: ERROR: active gamemaster is not registered yet\n", __func__);
        return;
    }

    if (n > GMPAYMENTS_SIGNATURES_TOTAL) {
        LogPrintf("%s: active gamemaster not in the top %d (%d)\n", __func__, GMPAYMENTS_SIGNATURES_TOTAL, n);
        return;
    }

    if (nBlockHeight <= nLastBlockHeight) return;

    if (g_budgetman.IsBudgetPaymentBlock(nBlockHeight)) {
        //is budget payment block -- handled by the budgeting software
        return;
    }

    // check winner height
    if (nBlockHeight - 100 > gamemasterman.GetBestHeight() + 1) {
        LogPrintf("%s: gmw - invalid height %d > %d", __func__, nBlockHeight - 100, gamemasterman.GetBestHeight() + 1);
        return;
    }

    // pay to the oldest GM that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    GamemasterRef pgm = gamemasterman.GetNextGamemasterInQueueForPayment(nBlockHeight, true, nCount);

    if (pgm == nullptr) {
        LogPrintf("%s: Failed to find gamemaster to pay\n", __func__);
        return;
    }

    CGamemasterPaymentWinner newWinner(gmVin, nBlockHeight);
    newWinner.AddPayee(pgm->GetPayeeScript());
    if (gmKey != nullopt) {
        // Legacy GM
        if (!newWinner.Sign(*gmKey, gmKey->GetPubKey().GetID())) {
            LogPrintf("%s: Failed to sign gamemaster winner\n", __func__);
            return;
        }
    } else {
        // DGM
        if (!newWinner.Sign(blsKey)) {
            LogPrintf("%s: Failed to sign gamemaster winner with DGM\n", __func__);
            return;
        }
    }

    AddWinningGamemaster(newWinner);
    newWinner.Relay();
    LogPrintf("%s: Relayed winner %s\n", __func__, newWinner.GetHash().ToString());
    nLastBlockHeight = nBlockHeight;
}

void CGamemasterPayments::Sync(CNode* node, int nCountNeeded)
{
    LOCK(cs_mapGamemasterPayeeVotes);

    int nHeight = gamemasterman.GetBestHeight();
    int nCount = (gamemasterman.CountEnabled() * 1.25);
    if (nCountNeeded > nCount) nCountNeeded = nCount;

    int nInvCount = 0;
    std::map<uint256, CGamemasterPaymentWinner>::iterator it = mapGamemasterPayeeVotes.begin();
    while (it != mapGamemasterPayeeVotes.end()) {
        CGamemasterPaymentWinner winner = (*it).second;
        if (winner.nBlockHeight >= nHeight - nCountNeeded && winner.nBlockHeight <= nHeight + 20) {
            node->PushInventory(CInv(MSG_GAMEMASTER_WINNER, winner.GetHash()));
            nInvCount++;
        }
        ++it;
    }
    g_connman->PushMessage(node, CNetMsgMaker(node->GetSendVersion()).Make(NetMsgType::SYNCSTATUSCOUNT, GAMEMASTER_SYNC_GMW, nInvCount));
}

std::string CGamemasterPayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapGamemasterPayeeVotes.size() << ", Blocks: " << (int)mapGamemasterBlocks.size();

    return info.str();
}

bool CGamemasterPayments::CanVote(const COutPoint& outGamemaster, int nBlockHeight) const
{
    LOCK(cs_mapGamemasterPayeeVotes);
    const auto it = mapGamemastersLastVote.find(outGamemaster);
    return it == mapGamemastersLastVote.end() || it->second != nBlockHeight;
}

void CGamemasterPayments::RecordWinnerVote(const COutPoint& outGamemaster, int nBlockHeight)
{
    LOCK(cs_mapGamemasterPayeeVotes);
    mapGamemastersLastVote[outGamemaster] = nBlockHeight;
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
            int nHeight = gamemasterman.GetBestHeight();
            CAmount nGmAmt = GetGamemasterPayment(nHeight);
            // if enforcement is disabled, there could be no gamemaster payment
            bool sporkEnforced = sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT);
            const std::string strError = strprintf("%s: invalid coinbase payment for gamemaster (%s vs expected=%s)",
                                                   __func__, FormatMoney(nCBaseOutAmt), FormatMoney(nGmAmt));
            if (sporkEnforced && nCBaseOutAmt != nGmAmt) {
                return _state.DoS(100, error(strError.c_str()), REJECT_INVALID, "bad-cb-amt");
            }
            if (!sporkEnforced && nCBaseOutAmt > nGmAmt) {
                return _state.DoS(100, error(strError.c_str()), REJECT_INVALID, "bad-cb-amt-spork8-disabled");
            }
            return true;
        }
    }
    return true;
}
