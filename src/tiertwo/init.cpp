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
#include "gamemasterman.h"
#include "gamemaster-payments.h"
#include "gamemasterconfig.h"
#include "llmq/quorums_init.h"
#include "scheduler.h"
#include "tiertwo/gamemaster_meta_manager.h"
#include "tiertwo/netfulfilledman.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/thread.hpp>

static std::unique_ptr<EvoNotificationInterface> pEvoNotificationInterface{nullptr};

std::string GetTierTwoHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup("Gamemaster options:");
    strUsage += HelpMessageOpt("-gamemaster=<n>", strprintf("Enable the client to act as a gamemaster (0-1, default: %u)", DEFAULT_GAMEMASTER));
    strUsage += HelpMessageOpt("-gmconf=<file>", strprintf("Specify gamemaster configuration file (default: %s)", hemis_GAMEMASTER_CONF_FILENAME));
    strUsage += HelpMessageOpt("-gmconflock=<n>", strprintf("Lock gamemasters from gamemaster configuration file (default: %u)", DEFAULT_GMCONFLOCK));
    strUsage += HelpMessageOpt("-gamemasterprivkey=<n>", "Set the gamemaster private key");
    strUsage += HelpMessageOpt("-gamemasteraddr=<n>", strprintf("Set external address:port to get to this gamemaster (example: %s). Only for Legacy Gamemasters", "128.127.106.235:49165"));
    strUsage += HelpMessageOpt("-budgetvotemode=<mode>", "Change automatic finalized budget voting behavior. mode=auto: Vote for only exact finalized budget match to my generated budget. (string, default: auto)");
    strUsage += HelpMessageOpt("-gmoperatorprivatekey=<bech32>", "Set the gamemaster operator private key. Only valid with -gamemaster=1. When set, the gamemaster acts as a deterministic gamemaster.");
    if (showDebug) {
        strUsage += HelpMessageOpt("-pushversion", strprintf("Modifies the gmauth serialization if the version is lower than %d."
                                                             "testnet/regtest only; ", GMAUTH_NODE_VER_VERSION));
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

    if (activeGamemasterManager) {
        UnregisterValidationInterface(activeGamemasterManager);
        delete activeGamemasterManager;
        activeGamemasterManager = nullptr;
    }
}

void InitTierTwoPreChainLoad(bool fReindex)
{
    int64_t nEvoDbCache = 1024 * 1024 * 64; // Max cache is 64MB
    deterministicGMManager.reset();
    evoDb.reset();
    evoDb.reset(new CEvoDB(nEvoDbCache, false, fReindex));
    deterministicGMManager.reset(new CDeterministicGMManager(*evoDb));
}

void InitTierTwoPostCoinsCacheLoad(CScheduler* scheduler)
{
    // Initialize LLMQ system
    llmq::InitLLMQSystem(*evoDb, scheduler, false);
}

void InitTierTwoChainTip()
{
    // force UpdatedBlockTip to initialize nCachedBlockHeight for DS, GM payments and budgets
    // but don't call it directly to prevent triggering of other listeners like zmq etc.
    pEvoNotificationInterface->InitializeCurrentBlockTip();
}

// Sets the last CACHED_BLOCK_HASHES hashes into gamemaster manager cache
static void LoadBlockHashesCache(CGamemasterMan& man)
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
    // ## Legacy Gamemasters Manager ### //
    // ################################# //
    uiInterface.InitMessage(_("Loading gamemaster cache..."));

    gamemasterman.SetBestHeight(chain_active_height);
    LoadBlockHashesCache(gamemasterman);
    CGamemasterDB gmdb;
    CGamemasterDB::ReadResult readResult = gmdb.Read(gamemasterman);
    if (readResult == CGamemasterDB::FileError)
        LogPrintf("Missing gamemaster cache file - gmcache.dat, will try to recreate\n");
    else if (readResult != CGamemasterDB::Ok) {
        LogPrintf("Error reading gmcache.dat - cached data discarded\n");
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
    // ## Legacy Gamemasters-Payments Manager ## //
    // ######################################### //
    uiInterface.InitMessage(_("Loading gamemaster payment cache..."));

    CGamemasterPaymentDB gmpayments;
    CGamemasterPaymentDB::ReadResult readResult3 = gmpayments.Read(gamemasterPayments);
    if (readResult3 == CGamemasterPaymentDB::FileError)
        LogPrintf("Missing gamemaster payment cache - gmpayments.dat, will try to recreate\n");
    else if (readResult3 != CGamemasterPaymentDB::Ok) {
        LogPrintf("Error reading gmpayments.dat - cached data discarded\n");
    }

    // ###################################### //
    // ## Legacy Parse 'gamemasters.conf'  ## //
    // ###################################### //
    std::string strErr;
    if (!gamemasterConfig.read(strErr)) {
        return UIError(strprintf(_("Error reading gamemaster configuration file: %s"), strErr));
    }

    // ############################## //
    // ## Net GMs Metadata Manager ## //
    // ############################## //
    uiInterface.InitMessage(_("Loading gamemaster cache..."));
    CFlatDB<CGamemasterMetaMan> metadb(GM_META_CACHE_FILENAME, GM_META_CACHE_FILE_ID);
    if (load_cache_files) {
        if (!metadb.Load(g_mmetaman)) {
            return UIError(strprintf(_("Failed to load gamemaster metadata cache from: %s"), metadb.GetDbPath().string()));
        }
    } else {
        CGamemasterMetaMan mmetamanTmp;
        if (!metadb.Dump(mmetamanTmp)) {
            return UIError(strprintf(_("Failed to clear gamemaster metadata cache at: %s"), metadb.GetDbPath().string()));
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
    RegisterValidationInterface(&gamemasterPayments);
    if (activeGamemasterManager) RegisterValidationInterface(activeGamemasterManager);
}

void DumpTierTwo()
{
    DumpGamemasters();
    DumpBudgets(g_budgetman);
    DumpGamemasterPayments();
    CFlatDB<CGamemasterMetaMan>(GM_META_CACHE_FILENAME, GM_META_CACHE_FILE_ID).Dump(g_mmetaman);
    CFlatDB<CNetFulfilledRequestManager>(NET_REQUESTS_CACHE_FILENAME, NET_REQUESTS_CACHE_FILE_ID).Dump(g_netfulfilledman);
}

void SetBudgetFinMode(const std::string& mode)
{
    g_budgetman.strBudgetMode = mode;
    LogPrintf("Budget Mode %s\n", g_budgetman.strBudgetMode);
}

bool InitActiveGM()
{
    fMasterNode = gArgs.GetBoolArg("-gamemaster", DEFAULT_GAMEMASTER);
    if ((fMasterNode || gamemasterConfig.getCount() > -1) && fTxIndex == false) {
        return UIError(strprintf(_("Enabling Gamemaster support requires turning on transaction indexing."
                                   "Please add %s to your configuration and start with %s"), "txindex=1", "-reindex"));
    }

    if (fMasterNode) {

        if (gArgs.IsArgSet("-connect") && gArgs.GetArgs("-connect").size() > 0) {
            return UIError(_("Cannot be a gamemaster and only connect to specific nodes"));
        }

        if (gArgs.GetArg("-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS) < DEFAULT_MAX_PEER_CONNECTIONS) {
            return UIError(strprintf(_("Gamemaster must be able to handle at least %d connections, set %s=%d"),
                                     DEFAULT_MAX_PEER_CONNECTIONS, "-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS));
        }

        const std::string& gmoperatorkeyStr = gArgs.GetArg("-gmoperatorprivatekey", "");
        const bool fDeterministic = !gmoperatorkeyStr.empty();
        LogPrintf("IS %s GAMEMASTER\n", (fDeterministic ? "DETERMINISTIC " : ""));

        if (fDeterministic) {
            // Check enforcement
            if (!deterministicGMManager->IsDIP3Enforced()) {
                const std::string strError = strprintf(
                        _("Cannot start deterministic gamemaster before enforcement. Remove %s to start as legacy gamemaster"),
                        "-gmoperatorprivatekey");
                LogPrintf("-- ERROR: %s\n", strError);
                return UIError(strError);
            }
            // Create and register activeGamemasterManager
            activeGamemasterManager = new CActiveDeterministicGamemasterManager();
            auto res = activeGamemasterManager->SetOperatorKey(gmoperatorkeyStr);
            if (!res) { return UIError(res.getError()); }
            // Init active gamemaster
            const CBlockIndex* pindexTip = WITH_LOCK(cs_main, return chainActive.Tip(););
            activeGamemasterManager->Init(pindexTip);
            if (activeGamemasterManager->GetState() == CActiveDeterministicGamemasterManager::GAMEMASTER_ERROR) {
                return UIError(activeGamemasterManager->GetStatus()); // state logged internally
            }
        } else {
            // Check enforcement
            if (deterministicGMManager->LegacyGMObsolete()) {
                const std::string strError = strprintf(
                        _("Legacy gamemaster system disabled. Use %s to start as deterministic gamemaster"),
                        "-gmoperatorprivatekey");
                LogPrintf("-- ERROR: %s\n", strError);
                return UIError(strError);
            }
            auto res = initGamemaster(gArgs.GetArg("-gamemasterprivkey", ""), gArgs.GetArg("-gamemasteraddr", ""),
                                      true);
            if (!res) { return UIError(res.getError()); }
        }
    }

#ifdef ENABLE_WALLET
    // !TODO: remove after complete transition to DGM
    // use only the first wallet here. This section can be removed after transition to DGM
    if (gArgs.GetBoolArg("-gmconflock", DEFAULT_GMCONFLOCK) && !vpwallets.empty() && vpwallets[0]) {
        LOCK(vpwallets[0]->cs_wallet);
        LogPrintf("Locking Gamemasters collateral utxo:\n");
        uint256 gmTxHash;
        for (const auto& gme : gamemasterConfig.getEntries()) {
            gmTxHash.SetHex(gme.getTxHash());
            COutPoint outpoint = COutPoint(gmTxHash, (unsigned int) std::stoul(gme.getOutputIndex()));
            vpwallets[0]->LockCoin(outpoint);
            LogPrintf("Locked collateral, GM: %s, tx hash: %s, output index: %s\n",
                      gme.getAlias(), gme.getTxHash(), gme.getOutputIndex());
        }
    }

    // automatic lock for DGM
    if (gArgs.GetBoolArg("-gmconflock", DEFAULT_GMCONFLOCK)) {
        LogPrintf("Locking gamemaster collaterals...\n");
        const auto& gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(false, [&](const CDeterministicGMCPtr& dgm) {
            for (CWallet* pwallet : vpwallets) {
                pwallet->LockOutpointIfMineWithMutex(nullptr, dgm->collateralOutpoint);
            }
        });
    }
#endif
    // All good
    return true;
}

void StartTierTwoThreadsAndScheduleJobs(boost::thread_group& threadGroup, CScheduler& scheduler)
{
    threadGroup.create_thread(std::bind(&ThreadCheckGamemasters));
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
    deterministicGMManager.reset();
    evoDb.reset();
}
