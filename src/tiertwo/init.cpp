// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "tiertwo/init.h"

#include "budget/budgetdb.h"
#include "evo/evodb.h"
#include "evo/evonotificationinterface.h"
#include "flatdb.h"
#include "guiinterface.h"
#include "guiinterfaceutil.h"
#include "masternodeman.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "llmq/quorums_init.h"
#include "scheduler.h"
#include "tiertwo/masternode_meta_manager.h"
#include "tiertwo/netfulfilledman.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/thread.hpp>

static std::unique_ptr<EvoNotificationInterface> pEvoNotificationInterface{nullptr};

std::string GetTierTwoHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup("Masternode options:");
    strUsage += HelpMessageOpt("-masternode=<n>", strprintf("Enable the client to act as a masternode (0-1, default: %u)", DEFAULT_MASTERNODE));
    strUsage += HelpMessageOpt("-mnconf=<file>", strprintf("Specify masternode configuration file (default: %s)", hemis_MASTERNODE_CONF_FILENAME));
    strUsage += HelpMessageOpt("-mnconflock=<n>", strprintf("Lock masternodes from masternode configuration file (default: %u)", DEFAULT_MNCONFLOCK));
    strUsage += HelpMessageOpt("-masternodeprivkey=<n>", "Set the masternode private key");
    strUsage += HelpMessageOpt("-masternodeaddr=<n>", strprintf("Set external address:port to get to this masternode (example: %s). Only for Legacy Masternodes", "128.127.106.235:51472"));
    strUsage += HelpMessageOpt("-budgetvotemode=<mode>", "Change automatic finalized budget voting behavior. mode=auto: Vote for only exact finalized budget match to my generated budget. (string, default: auto)");
    strUsage += HelpMessageOpt("-mnoperatorprivatekey=<bech32>", "Set the masternode operator private key. Only valid with -masternode=1. When set, the masternode acts as a deterministic masternode.");
    if (showDebug) {
        strUsage += HelpMessageOpt("-pushversion", strprintf("Modifies the mnauth serialization if the version is lower than %d."
                                                             "testnet/regtest only; ", MNAUTH_NODE_VER_VERSION));
        strUsage += HelpMessageOpt("-disabledkg", "Disable the DKG sessions process threads for the entire lifecycle. testnet/regtest only.");
    }
    return strUsage;
}

void InitTierTwoInterfaces()
{
    pEvoNotificationInterface = std::make_unique<EvoNotificationInterface>();
    RegisterValidationInterface(pEvoNotificationInterface.get());
}

void ResetTierTwoInterfaces()
{
    if (pEvoNotificationInterface) {
        UnregisterValidationInterface(pEvoNotificationInterface.get());
        pEvoNotificationInterface.reset();
    }

    if (activeMasternodeManager) {
        UnregisterValidationInterface(activeMasternodeManager);
        delete activeMasternodeManager;
        activeMasternodeManager = nullptr;
    }
}

void InitTierTwoPreChainLoad(bool fReindex)
{
    int64_t nEvoDbCache = 1024 * 1024 * 64; // Max cache is 64MB
    deterministicMNManager.reset();
    evoDb.reset();
    evoDb.reset(new CEvoDB(nEvoDbCache, false, fReindex));
    deterministicMNManager.reset(new CDeterministicMNManager(*evoDb));
}

void InitTierTwoPostCoinsCacheLoad(CScheduler* scheduler)
{
    // Initialize LLMQ system
    llmq::InitLLMQSystem(*evoDb, scheduler, false);
}

void InitTierTwoChainTip()
{
    // force UpdatedBlockTip to initialize nCachedBlockHeight for DS, MN payments and budgets
    // but don't call it directly to prevent triggering of other listeners like zmq etc.
    pEvoNotificationInterface->InitializeCurrentBlockTip();
}

// Sets the last CACHED_BLOCK_HASHES hashes into masternode manager cache
static void LoadBlockHashesCache(CMasternodeMan& man)
{
    LOCK(cs_main);
    const CBlockIndex* pindex = chainActive.Tip();
    unsigned int inserted = 0;
    while (pindex && inserted < CACHED_BLOCK_HASHES) {
        man.CacheBlockHash(pindex);
        pindex = pindex->pprev;
        ++inserted;
    }
}

bool LoadTierTwo(int chain_active_height, bool load_cache_files)
{
    // ################################# //
    // ## Legacy Masternodes Manager ### //
    // ################################# //
    uiInterface.InitMessage(_("Loading masternode cache..."));

    mnodeman.SetBestHeight(chain_active_height);
    LoadBlockHashesCache(mnodeman);
    CMasternodeDB mndb;
    CMasternodeDB::ReadResult readResult = mndb.Read(mnodeman);
    if (readResult == CMasternodeDB::FileError)
        LogPrintf("Missing masternode cache file - mncache.dat, will try to recreate\n");
    else if (readResult != CMasternodeDB::Ok) {
        LogPrintf("Error reading mncache.dat - cached data discarded\n");
    }

    // ##################### //
    // ## Budget Manager ### //
    // ##################### //
    uiInterface.InitMessage(_("Loading budget cache..."));

    CBudgetDB budgetdb;
    const bool fDryRun = (chain_active_height <= 0);
    if (!fDryRun) g_budgetman.SetBestHeight(chain_active_height);
    CBudgetDB::ReadResult readResult2 = budgetdb.Read(g_budgetman, fDryRun);

    if (readResult2 == CBudgetDB::FileError)
        LogPrintf("Missing budget cache - budget.dat, will try to recreate\n");
    else if (readResult2 != CBudgetDB::Ok) {
        LogPrintf("Error reading budget.dat - cached data discarded\n");
    }

    // flag our cached items so we send them to our peers
    g_budgetman.ResetSync();
    g_budgetman.ReloadMapSeen();

    // ######################################### //
    // ## Legacy Masternodes-Payments Manager ## //
    // ######################################### //
    uiInterface.InitMessage(_("Loading masternode payment cache..."));

    CMasternodePaymentDB mnpayments;
    CMasternodePaymentDB::ReadResult readResult3 = mnpayments.Read(masternodePayments);
    if (readResult3 == CMasternodePaymentDB::FileError)
        LogPrintf("Missing masternode payment cache - mnpayments.dat, will try to recreate\n");
    else if (readResult3 != CMasternodePaymentDB::Ok) {
        LogPrintf("Error reading mnpayments.dat - cached data discarded\n");
    }

    // ###################################### //
    // ## Legacy Parse 'masternodes.conf'  ## //
    // ###################################### //
    std::string strErr;
    if (!masternodeConfig.read(strErr)) {
        return UIError(strprintf(_("Error reading masternode configuration file: %s"), strErr));
    }

    // ############################## //
    // ## Net MNs Metadata Manager ## //
    // ############################## //
    uiInterface.InitMessage(_("Loading masternode cache..."));
    CFlatDB<CMasternodeMetaMan> metadb(MN_META_CACHE_FILENAME, MN_META_CACHE_FILE_ID);
    if (load_cache_files) {
        if (!metadb.Load(g_mmetaman)) {
            return UIError(strprintf(_("Failed to load masternode metadata cache from: %s"), metadb.GetDbPath().string()));
        }
    } else {
        CMasternodeMetaMan mmetamanTmp;
        if (!metadb.Dump(mmetamanTmp)) {
            return UIError(strprintf(_("Failed to clear masternode metadata cache at: %s"), metadb.GetDbPath().string()));
        }
    }

    // ############################## //
    // ## Network Requests Manager ## //
    // ############################## //
    uiInterface.InitMessage(_("Loading network requests cache..."));
    CFlatDB<CNetFulfilledRequestManager> netRequestsDb(NET_REQUESTS_CACHE_FILENAME, NET_REQUESTS_CACHE_FILE_ID);
    if (load_cache_files) {
        if (!netRequestsDb.Load(g_netfulfilledman)) {
            LogPrintf("Failed to load network requests cache from %s", netRequestsDb.GetDbPath().string());
        }
    } else {
        CNetFulfilledRequestManager netfulfilledmanTmp(0);
        if (!netRequestsDb.Dump(netfulfilledmanTmp)) {
            LogPrintf("Failed to clear network requests cache at %s", netRequestsDb.GetDbPath().string());
        }
    }

    return true;
}

void RegisterTierTwoValidationInterface()
{
    RegisterValidationInterface(&g_budgetman);
    RegisterValidationInterface(&masternodePayments);
    if (activeMasternodeManager) RegisterValidationInterface(activeMasternodeManager);
}

void DumpTierTwo()
{
    DumpMasternodes();
    DumpBudgets(g_budgetman);
    DumpMasternodePayments();
    CFlatDB<CMasternodeMetaMan>(MN_META_CACHE_FILENAME, MN_META_CACHE_FILE_ID).Dump(g_mmetaman);
    CFlatDB<CNetFulfilledRequestManager>(NET_REQUESTS_CACHE_FILENAME, NET_REQUESTS_CACHE_FILE_ID).Dump(g_netfulfilledman);
}

void SetBudgetFinMode(const std::string& mode)
{
    g_budgetman.strBudgetMode = mode;
    LogPrintf("Budget Mode %s\n", g_budgetman.strBudgetMode);
}

bool InitActiveMN()
{
    fMasterNode = gArgs.GetBoolArg("-masternode", DEFAULT_MASTERNODE);
    if ((fMasterNode || masternodeConfig.getCount() > -1) && fTxIndex == false) {
        return UIError(strprintf(_("Enabling Masternode support requires turning on transaction indexing."
                                   "Please add %s to your configuration and start with %s"), "txindex=1", "-reindex"));
    }

    if (fMasterNode) {

        if (gArgs.IsArgSet("-connect") && gArgs.GetArgs("-connect").size() > 0) {
            return UIError(_("Cannot be a masternode and only connect to specific nodes"));
        }

        if (gArgs.GetArg("-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS) < DEFAULT_MAX_PEER_CONNECTIONS) {
            return UIError(strprintf(_("Masternode must be able to handle at least %d connections, set %s=%d"),
                                     DEFAULT_MAX_PEER_CONNECTIONS, "-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS));
        }

        const std::string& mnoperatorkeyStr = gArgs.GetArg("-mnoperatorprivatekey", "");
        const bool fDeterministic = !mnoperatorkeyStr.empty();
        LogPrintf("IS %s MASTERNODE\n", (fDeterministic ? "DETERMINISTIC " : ""));

        if (fDeterministic) {
            // Check enforcement
            if (!deterministicMNManager->IsDIP3Enforced()) {
                const std::string strError = strprintf(
                        _("Cannot start deterministic masternode before enforcement. Remove %s to start as legacy masternode"),
                        "-mnoperatorprivatekey");
                LogPrintf("-- ERROR: %s\n", strError);
                return UIError(strError);
            }
            // Create and register activeMasternodeManager
            activeMasternodeManager = new CActiveDeterministicMasternodeManager();
            auto res = activeMasternodeManager->SetOperatorKey(mnoperatorkeyStr);
            if (!res) { return UIError(res.getError()); }
            // Init active masternode
            const CBlockIndex* pindexTip = WITH_LOCK(cs_main, return chainActive.Tip(););
            activeMasternodeManager->Init(pindexTip);
            if (activeMasternodeManager->GetState() == CActiveDeterministicMasternodeManager::MASTERNODE_ERROR) {
                return UIError(activeMasternodeManager->GetStatus()); // state logged internally
            }
        } else {
            // Check enforcement
            if (deterministicMNManager->LegacyMNObsolete()) {
                const std::string strError = strprintf(
                        _("Legacy masternode system disabled. Use %s to start as deterministic masternode"),
                        "-mnoperatorprivatekey");
                LogPrintf("-- ERROR: %s\n", strError);
                return UIError(strError);
            }
            auto res = initMasternode(gArgs.GetArg("-masternodeprivkey", ""), gArgs.GetArg("-masternodeaddr", ""),
                                      true);
            if (!res) { return UIError(res.getError()); }
        }
    }

#ifdef ENABLE_WALLET
    // !TODO: remove after complete transition to DMN
    // use only the first wallet here. This section can be removed after transition to DMN
    if (gArgs.GetBoolArg("-mnconflock", DEFAULT_MNCONFLOCK) && !vpwallets.empty() && vpwallets[0]) {
        LOCK(vpwallets[0]->cs_wallet);
        LogPrintf("Locking Masternodes collateral utxo:\n");
        uint256 mnTxHash;
        for (const auto& mne : masternodeConfig.getEntries()) {
            mnTxHash.SetHex(mne.getTxHash());
            COutPoint outpoint = COutPoint(mnTxHash, (unsigned int) std::stoul(mne.getOutputIndex()));
            vpwallets[0]->LockCoin(outpoint);
            LogPrintf("Locked collateral, MN: %s, tx hash: %s, output index: %s\n",
                      mne.getAlias(), mne.getTxHash(), mne.getOutputIndex());
        }
    }

    // automatic lock for DMN
    if (gArgs.GetBoolArg("-mnconflock", DEFAULT_MNCONFLOCK)) {
        LogPrintf("Locking masternode collaterals...\n");
        const auto& mnList = deterministicMNManager->GetListAtChainTip();
        mnList.ForEachMN(false, [&](const CDeterministicMNCPtr& dmn) {
            for (CWallet* pwallet : vpwallets) {
                pwallet->LockOutpointIfMineWithMutex(nullptr, dmn->collateralOutpoint);
            }
        });
    }
#endif
    // All good
    return true;
}

void StartTierTwoThreadsAndScheduleJobs(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    threadGroup.create_thread(std::bind(&ThreadCheckMasternodes));
    scheduler.scheduleEvery(std::bind(&CNetFulfilledRequestManager::DoMaintenance, std::ref(g_netfulfilledman)), 60 * 1000);

    // Start LLMQ system
    if (gArgs.GetBoolArg("-disabledkg", false)) {
        if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
            throw std::runtime_error("DKG system can be disabled only on testnet/regtest");
        } else {
            LogPrintf("DKG system disabled.\n");
        }
    } else {
        llmq::StartLLMQSystem();
    }
}

void StopTierTwoThreads()
{
    llmq::StopLLMQSystem();
}

void DeleteTierTwo()
{
    llmq::DestroyLLMQSystem();
    deterministicMNManager.reset();
    evoDb.reset();
}
