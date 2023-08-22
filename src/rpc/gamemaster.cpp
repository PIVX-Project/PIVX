// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activegamemaster.h"
#include "db.h"
#include "evo/deterministicgms.h"
#include "key_io.h"
#include "gamemaster-payments.h"
#include "gamemasterconfig.h"
#include "gamemasterman.h"
#include "netbase.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "rpc/server.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/rpcwallet.h"
#endif

#include <univalue.h>

#include <boost/tokenizer.hpp>

// Duplicated from rpcevo.cpp for the compatibility phase. Remove after v6
static UniValue DgmToJson(const CDeterministicGMCPtr dgm)
{
    UniValue ret(UniValue::VOBJ);
    dgm->ToJson(ret);
    Coin coin;
    if (!WITH_LOCK(cs_main, return pcoinsTip->GetUTXOCoin(dgm->collateralOutpoint, coin); )) {
        return ret;
    }
    CTxDestination dest;
    if (!ExtractDestination(coin.out.scriptPubKey, dest)) {
        return ret;
    }
    ret.pushKV("collateralAddress", EncodeDestination(dest));
    return ret;
}

UniValue gmping(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty()) {
        throw std::runtime_error(
            "gmping \n"
            "\nSend gamemaster ping. Only for remote gamemasters on Regtest\n"

            "\nResult:\n"
            "{\n"
            "  \"sent\":           (string YES|NO) Whether the ping was sent and, if not, the error.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("gmping", "") + HelpExampleRpc("gmping", ""));
    }

    if (!Params().IsRegTestNet()) {
        throw JSONRPCError(RPC_MISC_ERROR, "command available only for RegTest network");
    }

    if (!fGameMaster) {
        throw JSONRPCError(RPC_MISC_ERROR, "this is not a gamemaster");
    }

    UniValue ret(UniValue::VOBJ);
    std::string strError;
    ret.pushKV("sent", activeGamemaster.SendGamemasterPing(strError) ?
                       "YES" : strprintf("NO (%s)", strError));
    return ret;
}

UniValue initgamemaster(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() < 1 || request.params.size() > 2)) {
        throw std::runtime_error(
                "initgamemaster \"privkey\" ( \"address\" )\n"
                "\nInitialize gamemaster on demand if it's not already initialized.\n"
                "\nArguments:\n"
                "1. privkey          (string, required) The gamemaster private key.\n"
                "2. address          (string, optional) The IP:Port of the gamemaster. (Only needed for legacy gamemasters)\n"

                "\nResult:\n"
                " success            (string) if the gamemaster initialization succeeded.\n"

                "\nExamples:\n" +
                HelpExampleCli("initgamemaster", "\"9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK\" \"187.24.32.124:49165\"") +
                HelpExampleRpc("initgamemaster", "\"bls-sk1xye8es37kk7y2mz7mad6yz7fdygttexqwhypa0u86hzw2crqgxfqy29ajm\""));
    }

    std::string _strGameMasterPrivKey = request.params[0].get_str();
    if (_strGameMasterPrivKey.empty()) throw JSONRPCError(RPC_INVALID_PARAMETER, "Gamemaster key cannot be empty.");

    const auto& params = Params();
    bool isDeterministic = _strGameMasterPrivKey.find(params.Bech32HRP(CChainParams::BLS_SECRET_KEY)) != std::string::npos;
    if (isDeterministic) {
        if (!activeGamemasterManager) {
            activeGamemasterManager = new CActiveDeterministicGamemasterManager();
            RegisterValidationInterface(activeGamemasterManager);
        }
        auto res = activeGamemasterManager->SetOperatorKey(_strGameMasterPrivKey);
        if (!res) throw std::runtime_error(res.getError());
        const CBlockIndex* pindexTip = WITH_LOCK(cs_main, return chainActive.Tip(); );
        activeGamemasterManager->Init(pindexTip);
        if (activeGamemasterManager->GetState() == CActiveDeterministicGamemasterManager::GAMEMASTER_ERROR) {
            throw std::runtime_error(activeGamemasterManager->GetStatus());
        }
        return "success";
    }
    // legacy
    if (request.params.size() < 2) throw JSONRPCError(RPC_INVALID_PARAMETER, "Must specify the IP address for legacy gm");
    std::string _strGameMasterAddr = request.params[1].get_str();
    auto res = initGamemaster(_strGameMasterPrivKey, _strGameMasterAddr, false);
    if (!res) throw std::runtime_error(res.getError());
    return "success";
}

UniValue getcachedblockhashes(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "getcachedblockhashes \n"
            "\nReturn the block hashes cached in the gamemaster manager\n"

            "\nResult:\n"
            "[\n"
            "  ...\n"
            "  \"xxxx\",   (string) hash at Index d (height modulo max cache size)\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getcachedblockhashes", "") + HelpExampleRpc("getcachedblockhashes", ""));

    std::vector<uint256> vCacheCopy = gamemasterman.GetCachedBlocks();
    UniValue ret(UniValue::VARR);
    for (int i = 0; (unsigned) i < vCacheCopy.size(); i++) {
        ret.push_back(vCacheCopy[i].ToString());
    }
    return ret;
}

static inline bool filter(const std::string& str, const std::string& strFilter)
{
    return str.find(strFilter) != std::string::npos;
}

static inline bool filterGamemaster(const UniValue& dgmo, const std::string& strFilter, bool fEnabled)
{
    return strFilter.empty() || (filter("ENABLED", strFilter) && fEnabled)
                             || (filter("POSE_BANNED", strFilter) && !fEnabled)
                             || (filter(dgmo["proTxHash"].get_str(), strFilter))
                             || (filter(dgmo["collateralHash"].get_str(), strFilter))
                             || (filter(dgmo["collateralAddress"].get_str(), strFilter))
                             || (filter(dgmo["dgmstate"]["ownerAddress"].get_str(), strFilter))
                             || (filter(dgmo["dgmstate"]["operatorPubKey"].get_str(), strFilter))
                             || (filter(dgmo["dgmstate"]["votingAddress"].get_str(), strFilter));
}

UniValue listgamemasters(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 1))
        throw std::runtime_error(
            "listgamemasters ( \"filter\" )\n"
            "\nGet a ranked list of gamemasters\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match by txhash, status, or addr.\n"

            // !TODO: update for DGMs
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"rank\": n,                             (numeric) Gamemaster Rank (or 0 if not enabled)\n"
            "    \"type\": \"legacy\"|\"deterministic\",  (string) type of gamemaster\n"
            "    \"txhash\": \"hash\",                    (string) Collateral transaction hash\n"
            "    \"outidx\": n,                           (numeric) Collateral transaction output index\n"
            "    \"pubkey\": \"key\",                     (string) Gamemaster public key used for message broadcasting\n"
            "    \"status\": s,                           (string) Status (ENABLED/EXPIRED/REMOVE/etc)\n"
            "    \"addr\": \"addr\",                      (string) Gamemaster hemis address\n"
            "    \"version\": v,                          (numeric) Gamemaster protocol version\n"
            "    \"lastseen\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last seen\n"
            "    \"activetime\": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) gamemaster has been active\n"
            "    \"lastpaid\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) gamemaster was last paid\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listgamemasters", "") + HelpExampleRpc("listgamemasters", ""));


    const std::string& strFilter = request.params.size() > 0 ? request.params[0].get_str() : "";
    UniValue ret(UniValue::VARR);

    if (deterministicGMManager->LegacyGMObsolete()) {
        auto gmList = deterministicGMManager->GetListAtChainTip();
        gmList.ForEachGM(false, [&](const CDeterministicGMCPtr& dgm) {
            UniValue obj = DgmToJson(dgm);
            if (filterGamemaster(obj, strFilter, !dgm->IsPoSeBanned())) {
                ret.push_back(obj);
            }
        });
        return ret;
    }

    // Legacy gamemasters (!TODO: remove when transition to dgm is complete)
    const CBlockIndex* chainTip = GetChainTip();
    if (!chainTip) return "[]";
    int nHeight = chainTip->nHeight;
    auto gmList = deterministicGMManager->GetListAtChainTip();

    int count_enabled = gamemasterman.CountEnabled();
    std::vector<std::pair<int64_t, GamemasterRef>> vGamemasterRanks = gamemasterman.GetGamemasterRanks(nHeight);
    for (int pos=0; pos < (int) vGamemasterRanks.size(); pos++) {
        const auto& s = vGamemasterRanks[pos];
        UniValue obj(UniValue::VOBJ);
        const CGamemaster& gm = *(s.second);

        if (!gm.gmPayeeScript.empty()) {
            // Deterministic gamemaster
            auto dgm = gmList.GetGMByCollateral(gm.vin.prevout);
            if (dgm) {
                UniValue obj = DgmToJson(dgm);
                bool fEnabled = !dgm->IsPoSeBanned();
                if (filterGamemaster(obj, strFilter, fEnabled)) {
                    // Added for backward compatibility with legacy gamemasters
                    obj.pushKV("type", "deterministic");
                    obj.pushKV("txhash", obj["proTxHash"].get_str());
                    obj.pushKV("addr", obj["dgmstate"]["payoutAddress"].get_str());
                    obj.pushKV("status", fEnabled ? "ENABLED" : "POSE_BANNED");
                    obj.pushKV("rank", fEnabled ? pos : 0);
                    ret.push_back(obj);
                }
            }
            continue;
        }

        std::string strVin = gm.vin.prevout.ToStringShort();
        std::string strTxHash = gm.vin.prevout.hash.ToString();
        uint32_t oIdx = gm.vin.prevout.n;

        if (strFilter != "" && strTxHash.find(strFilter) == std::string::npos &&
            gm.Status().find(strFilter) == std::string::npos &&
            EncodeDestination(gm.pubKeyCollateralAddress.GetID()).find(strFilter) == std::string::npos) continue;

        std::string strStatus = gm.Status();
        std::string strHost;
        int port;
        SplitHostPort(gm.addr.ToString(), port, strHost);
        CNetAddr node;
        LookupHost(strHost.c_str(), node, false);
        std::string strNetwork = GetNetworkName(node.GetNetwork());

        obj.pushKV("rank", (strStatus == "ENABLED" ? pos : -1));
        obj.pushKV("type", "legacy");
        obj.pushKV("network", strNetwork);
        obj.pushKV("txhash", strTxHash);
        obj.pushKV("outidx", (uint64_t)oIdx);
        obj.pushKV("pubkey", EncodeDestination(gm.pubKeyGamemaster.GetID()));
        obj.pushKV("status", strStatus);
        obj.pushKV("addr", EncodeDestination(gm.pubKeyCollateralAddress.GetID()));
        obj.pushKV("version", gm.protocolVersion);
        obj.pushKV("lastseen", (int64_t)gm.lastPing.sigTime);
        obj.pushKV("activetime", (int64_t)(gm.lastPing.sigTime - gm.sigTime));
        obj.pushKV("lastpaid", (int64_t)gamemasterman.GetLastPaid(s.second, count_enabled, chainTip));

        ret.push_back(obj);
    }

    return ret;
}

UniValue getgamemastercount (const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 0))
        throw std::runtime_error(
            "getgamemastercount\n"
            "\nGet gamemaster count values\n"

            "\nResult:\n"
            "{\n"
            "  \"total\": n,        (numeric) Total gamemasters\n"
            "  \"stable\": n,       (numeric) Stable count\n"
            "  \"enabled\": n,      (numeric) Enabled gamemasters\n"
            "  \"inqueue\": n,      (numeric) Gamemasters in queue\n"
            "  \"ipv4\": n,         (numeric) Number of IPv4 gamemasters\n"
            "  \"ipv6\": n,         (numeric) Number of IPv6 gamemasters\n"
            "  \"onion\": n         (numeric) Number of Tor gamemasters\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getgamemastercount", "") + HelpExampleRpc("getgamemastercount", ""));

    UniValue obj(UniValue::VOBJ);
    int nCount = 0;
    const CBlockIndex* pChainTip = GetChainTip();
    if (!pChainTip) return "unknown";

    gamemasterman.GetNextGamemasterInQueueForPayment(pChainTip->nHeight, true, nCount, pChainTip);
    auto infoGMs = gamemasterman.getGMsInfo();

    obj.pushKV("total", infoGMs.total);
    obj.pushKV("stable", infoGMs.stableSize);
    obj.pushKV("enabled", infoGMs.enabledSize);
    obj.pushKV("inqueue", nCount);
    obj.pushKV("ipv4", infoGMs.ipv4);
    obj.pushKV("ipv6", infoGMs.ipv6);
    obj.pushKV("onion", infoGMs.onion);

    return obj;
}

UniValue gamemastercurrent(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 0))
        throw std::runtime_error(
            "gamemastercurrent\n"
            "\nGet current gamemaster winner (scheduled to be paid next).\n"

            "\nResult:\n"
            "{\n"
            "  \"protocol\": xxxx,        (numeric) Protocol version\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"pubkey\": \"xxxx\",      (string) GM Public key\n"
            "  \"lastseen\": xxx,         (numeric) Time since epoch of last seen\n"
            "  \"activeseconds\": xxx,    (numeric) Seconds GM has been active\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("gamemastercurrent", "") + HelpExampleRpc("gamemastercurrent", ""));

    const CBlockIndex* pChainTip = GetChainTip();
    if (!pChainTip) return "unknown";

    int nCount = 0;
    GamemasterRef winner = gamemasterman.GetNextGamemasterInQueueForPayment(pChainTip->nHeight + 1, true, nCount, pChainTip);
    if (winner) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("protocol", (int64_t)winner->protocolVersion);
        obj.pushKV("txhash", winner->vin.prevout.hash.ToString());
        obj.pushKV("pubkey", EncodeDestination(winner->pubKeyCollateralAddress.GetID()));
        obj.pushKV("lastseen", winner->lastPing.IsNull() ? winner->sigTime : (int64_t)winner->lastPing.sigTime);
        obj.pushKV("activeseconds", winner->lastPing.IsNull() ? 0 : (int64_t)(winner->lastPing.sigTime - winner->sigTime));
        return obj;
    }

    throw std::runtime_error("unknown");
}

bool StartGamemasterEntry(UniValue& statusObjRet, CGamemasterBroadcast& gmbRet, bool& fSuccessRet, const CGamemasterConfig::CGamemasterEntry& gme, std::string& errorMessage, std::string strCommand = "")
{
    int nIndex;
    if(!gme.castOutputIndex(nIndex)) {
        return false;
    }

    CTxIn vin = CTxIn(uint256S(gme.getTxHash()), uint32_t(nIndex));
    CGamemaster* pgm = gamemasterman.Find(vin.prevout);
    if (pgm != nullptr) {
        if (strCommand == "missing") return false;
        if (strCommand == "disabled" && pgm->IsEnabled()) return false;
    }

    fSuccessRet = CGamemasterBroadcast::Create(gme.getIp(), gme.getPrivKey(), gme.getTxHash(), gme.getOutputIndex(), errorMessage, gmbRet, false, gamemasterman.GetBestHeight());

    statusObjRet.pushKV("alias", gme.getAlias());
    statusObjRet.pushKV("result", fSuccessRet ? "success" : "failed");
    statusObjRet.pushKV("error", fSuccessRet ? "" : errorMessage);

    return true;
}

void RelayGMB(CGamemasterBroadcast& gmb, const bool fSuccess, int& successful, int& failed)
{
    if (fSuccess) {
        successful++;
        gamemasterman.UpdateGamemasterList(gmb);
        gmb.Relay();
    } else {
        failed++;
    }
}

void RelayGMB(CGamemasterBroadcast& gmb, const bool fSucces)
{
    int successful = 0, failed = 0;
    return RelayGMB(gmb, fSucces, successful, failed);
}

void SerializeGMB(UniValue& statusObjRet, const CGamemasterBroadcast& gmb, const bool fSuccess, int& successful, int& failed)
{
    if(fSuccess) {
        successful++;
        CDataStream ssGmb(SER_NETWORK, PROTOCOL_VERSION);
        ssGmb << gmb;
        statusObjRet.pushKV("hex", HexStr(ssGmb));
    } else {
        failed++;
    }
}

void SerializeGMB(UniValue& statusObjRet, const CGamemasterBroadcast& gmb, const bool fSuccess)
{
    int successful = 0, failed = 0;
    return SerializeGMB(statusObjRet, gmb, fSuccess, successful, failed);
}

UniValue startgamemaster(const JSONRPCRequest& request)
{
    // Skip after legacy obsolete. !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        throw JSONRPCError(RPC_MISC_ERROR, "startgamemaster is not supported when deterministic gamemaster list is active (DIP3)");
    }

    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    std::string strCommand;
    if (!request.params.empty()) {
        strCommand = request.params[0].get_str();
    }

    if (strCommand == "local")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Local start is deprecated. Start your gamemaster from the controller wallet instead.");
    if (strCommand == "many")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Many set is deprecated. Use either 'all', 'missing', or 'disabled'.");

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4 ||
        (strCommand == "alias" && request.params.size() < 3))
        throw std::runtime_error(
            "startgamemaster \"all|missing|disabled|alias\" lock_wallet ( \"alias\" reload_conf )\n"
            "\nAttempts to start one or more gamemaster(s)\n" +
            HelpRequiringPassphrase(pwallet) + "\n"

            "\nArguments:\n"
            "1. set          (string, required) Specify which set of gamemaster(s) to start.\n"
            "2. lock_wallet  (boolean, required) Lock wallet after completion.\n"
            "3. alias        (string, optional) Gamemaster alias. Required if using 'alias' as the set.\n"
            "4. reload_conf  (boolean, optional, default=False) reload the gamemasters.conf data from disk"

            "\nResult:\n"
            "{\n"
            "  \"overall\": \"xxxx\",     (string) Overall status message\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"alias\": \"xxxx\",   (string) Node alias\n"
            "      \"result\": \"xxxx\",  (string) 'success' or 'failed'\n"
            "      \"error\": \"xxxx\"    (string) Error message, if failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("startgamemaster", "\"alias\" false \"my_gm\"") + HelpExampleRpc("startgamemaster", "\"alias\" false \"my_gm\""));

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL, UniValue::VSTR, UniValue::VBOOL}, true);

    EnsureWalletIsUnlocked(pwallet);

    bool fLock = request.params[1].get_bool();
    bool fReload = request.params.size() > 3 ? request.params[3].get_bool() : false;

    // Check reload param
    if (fReload) {
        gamemasterConfig.clear();
        std::string error;
        if (!gamemasterConfig.read(error)) {
            throw std::runtime_error("Error reloading gamemaster.conf, " + error);
        }
    }

    if (strCommand == "all" || strCommand == "missing" || strCommand == "disabled") {
        if ((strCommand == "missing" || strCommand == "disabled") &&
            (g_tiertwo_sync_state.GetSyncPhase() <= GAMEMASTER_SYNC_LIST ||
                    g_tiertwo_sync_state.GetSyncPhase() == GAMEMASTER_SYNC_FAILED)) {
            throw std::runtime_error("You can't use this command until gamemaster list is synced\n");
        }

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        for (const CGamemasterConfig::CGamemasterEntry& gme : gamemasterConfig.getEntries()) {
            UniValue statusObj(UniValue::VOBJ);
            CGamemasterBroadcast gmb;
            std::string errorMessage;
            bool fSuccess = false;
            if (!StartGamemasterEntry(statusObj, gmb, fSuccess, gme, errorMessage, strCommand))
                continue;
            resultsObj.push_back(statusObj);
            RelayGMB(gmb, fSuccess, successful, failed);
        }
        if (fLock)
            pwallet->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.pushKV("overall", strprintf("Successfully started %d gamemasters, failed to start %d, total %d", successful, failed, successful + failed));
        returnObj.pushKV("detail", resultsObj);

        return returnObj;
    }

    if (strCommand == "alias") {
        std::string alias = request.params[2].get_str();

        bool found = false;

        UniValue resultsObj(UniValue::VARR);
        UniValue statusObj(UniValue::VOBJ);

        for (const CGamemasterConfig::CGamemasterEntry& gme : gamemasterConfig.getEntries()) {
            if (gme.getAlias() == alias) {
                CGamemasterBroadcast gmb;
                found = true;
                std::string errorMessage;
                bool fSuccess = false;
                if (!StartGamemasterEntry(statusObj, gmb, fSuccess, gme, errorMessage, strCommand))
                    continue;
                RelayGMB(gmb, fSuccess);
                break;
            }
        }

        if (fLock)
            pwallet->Lock();

        if(!found) {
            statusObj.pushKV("alias", alias);
            statusObj.pushKV("result", "failed");
            statusObj.pushKV("error", "Could not find alias in config. Verify with listgamemasterconf.");
        }

        return statusObj;
    }
    throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid set name %s.", strCommand));
}

UniValue creategamemasterkey(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 0))
        throw std::runtime_error(
            "creategamemasterkey\n"
            "\nCreate a new gamemaster private key\n"

            "\nResult:\n"
            "\"key\"    (string) Gamemaster private key\n"

            "\nExamples:\n" +
            HelpExampleCli("creategamemasterkey", "") + HelpExampleRpc("creategamemasterkey", ""));

    CKey secret;
    secret.MakeNewKey(false);

    return KeyIO::EncodeSecret(secret);
}

UniValue getgamemasteroutputs(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || (request.params.size() != 0))
        throw std::runtime_error(
            "getgamemasteroutputs\n"
            "\nPrint all gamemaster transaction outputs\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txhash\": \"xxxx\",    (string) output transaction hash\n"
            "    \"outputidx\": n       (numeric) output index number\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getgamemasteroutputs", "") + HelpExampleRpc("getgamemasteroutputs", ""));

    // Find possible candidates
    CWallet::AvailableCoinsFilter coinsFilter;
    coinsFilter.fIncludeDelegated = false;
    coinsFilter.nMaxOutValue = Params().GetConsensus().nGMCollateralAmt;
    coinsFilter.nMinOutValue = coinsFilter.nMaxOutValue;
    coinsFilter.fIncludeLocked = true;
    std::vector<COutput> possibleCoins;
    pwallet->AvailableCoins(&possibleCoins, nullptr, coinsFilter);

    UniValue ret(UniValue::VARR);
    for (COutput& out : possibleCoins) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("txhash", out.tx->GetHash().ToString());
        obj.pushKV("outputidx", out.i);
        ret.push_back(obj);
    }

    return ret;
}

UniValue listgamemasterconf(const JSONRPCRequest& request)
{
    std::string strFilter = "";

    if (request.params.size() == 1) strFilter = request.params[0].get_str();

    if (request.fHelp || (request.params.size() > 1))
        throw std::runtime_error(
            "listgamemasterconf ( \"filter\" )\n"
            "\nPrint gamemaster.conf in JSON format\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match on alias, address, txHash, or status.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"alias\": \"xxxx\",        (string) gamemaster alias\n"
            "    \"address\": \"xxxx\",      (string) gamemaster IP address\n"
            "    \"privateKey\": \"xxxx\",   (string) gamemaster private key\n"
            "    \"txHash\": \"xxxx\",       (string) transaction hash\n"
            "    \"outputIndex\": n,       (numeric) transaction output index\n"
            "    \"status\": \"xxxx\"        (string) gamemaster status\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listgamemasterconf", "") + HelpExampleRpc("listgamemasterconf", ""));

    std::vector<CGamemasterConfig::CGamemasterEntry> gmEntries;
    gmEntries = gamemasterConfig.getEntries();

    UniValue ret(UniValue::VARR);

    for (CGamemasterConfig::CGamemasterEntry gme : gamemasterConfig.getEntries()) {
        int nIndex;
        if(!gme.castOutputIndex(nIndex))
            continue;
        CTxIn vin = CTxIn(uint256S(gme.getTxHash()), uint32_t(nIndex));
        CGamemaster* pgm = gamemasterman.Find(vin.prevout);

        std::string strStatus = pgm ? pgm->Status() : "MISSING";

        if (strFilter != "" && gme.getAlias().find(strFilter) == std::string::npos &&
            gme.getIp().find(strFilter) == std::string::npos &&
            gme.getTxHash().find(strFilter) == std::string::npos &&
            strStatus.find(strFilter) == std::string::npos) continue;

        UniValue gmObj(UniValue::VOBJ);
        gmObj.pushKV("alias", gme.getAlias());
        gmObj.pushKV("address", gme.getIp());
        gmObj.pushKV("privateKey", gme.getPrivKey());
        gmObj.pushKV("txHash", gme.getTxHash());
        gmObj.pushKV("outputIndex", gme.getOutputIndex());
        gmObj.pushKV("status", strStatus);
        ret.push_back(gmObj);
    }

    return ret;
}

UniValue getgamemasterstatus(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 0))
        throw std::runtime_error(
            "getgamemasterstatus\n"
            "\nPrint gamemaster status\n"

            "\nResult (if legacy gamemaster):\n"
            "{\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"outputidx\": n,          (numeric) Collateral transaction output index number\n"
            "  \"netaddr\": \"xxxx\",     (string) Gamemaster network address\n"
            "  \"addr\": \"xxxx\",        (string) hemis address for gamemaster payments\n"
            "  \"status\": \"xxxx\",      (string) Gamemaster status\n"
            "  \"message\": \"xxxx\"      (string) Gamemaster status message\n"
            "}\n"
            "\n"
            "\nResult (if deterministic gamemaster):\n"
            "{\n"
            "... !TODO ...\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getgamemasterstatus", "") + HelpExampleRpc("getgamemasterstatus", ""));

    if (!fGameMaster)
        throw JSONRPCError(RPC_MISC_ERROR, _("This is not a gamemaster."));

    bool fLegacyGM = (activeGamemaster.vin != nullopt);
    bool fDeterministicGM = (activeGamemasterManager != nullptr);

    if (!fLegacyGM && !fDeterministicGM) {
        throw JSONRPCError(RPC_MISC_ERROR, _("Active Gamemaster not initialized."));
    }

    if (fDeterministicGM) {
        if (!deterministicGMManager->IsDIP3Enforced()) {
            // this should never happen as ProTx transactions are not accepted yet
            throw JSONRPCError(RPC_MISC_ERROR, _("Deterministic gamemasters are not enforced yet"));
        }
        const CActiveGamemasterInfo* agminfo = activeGamemasterManager->GetInfo();
        UniValue gmObj(UniValue::VOBJ);
        auto dgm = deterministicGMManager->GetListAtChainTip().GetGMByOperatorKey(agminfo->pubKeyOperator);
        if (dgm) {
            dgm->ToJson(gmObj);
        }
        gmObj.pushKV("netaddr", agminfo->service.ToString());
        gmObj.pushKV("status", activeGamemasterManager->GetStatus());
        return gmObj;
    }

    // Legacy code !TODO: remove when transition to DGM is complete
    if (deterministicGMManager->LegacyGMObsolete()) {
        throw JSONRPCError(RPC_MISC_ERROR, _("Legacy Gamemaster is obsolete."));
    }

    CGamemaster* pgm = gamemasterman.Find(activeGamemaster.vin->prevout);

    if (pgm) {
        UniValue gmObj(UniValue::VOBJ);
        gmObj.pushKV("txhash", activeGamemaster.vin->prevout.hash.ToString());
        gmObj.pushKV("outputidx", (uint64_t)activeGamemaster.vin->prevout.n);
        gmObj.pushKV("netaddr", activeGamemaster.service.ToString());
        gmObj.pushKV("addr", EncodeDestination(pgm->pubKeyCollateralAddress.GetID()));
        gmObj.pushKV("status", activeGamemaster.GetStatus());
        gmObj.pushKV("message", activeGamemaster.GetStatusMessage());
        return gmObj;
    }
    throw std::runtime_error("Gamemaster not found in the list of available gamemasters. Current status: "
                        + activeGamemaster.GetStatusMessage());
}

UniValue getgamemasterwinners(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getgamemasterwinners ( blocks \"filter\" )\n"
            "\nPrint the gamemaster winners for the last n blocks\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Number of previous blocks to show (default: 10)\n"
            "2. filter      (string, optional) Search filter matching GM address\n"

            "\nResult (single winner):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": {\n"
            "      \"address\": \"xxxx\",    (string) hemis GM Address\n"
            "      \"nVotes\": n,          (numeric) Number of votes for winner\n"
            "    }\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nResult (multiple winners):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": [\n"
            "      {\n"
            "        \"address\": \"xxxx\",  (string) hemis GM Address\n"
            "        \"nVotes\": n,        (numeric) Number of votes for winner\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getgamemasterwinners", "") + HelpExampleRpc("getgamemasterwinners", ""));

    int nHeight = WITH_LOCK(cs_main, return chainActive.Height());
    if (nHeight < 0) return "[]";

    int nLast = 10;
    std::string strFilter = "";

    if (request.params.size() >= 1)
        nLast = atoi(request.params[0].get_str());

    if (request.params.size() == 2)
        strFilter = request.params[1].get_str();

    UniValue ret(UniValue::VARR);

    for (int i = nHeight - nLast; i < nHeight + 20; i++) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("nHeight", i);

        std::string strPayment = GetRequiredPaymentsString(i);
        if (strFilter != "" && strPayment.find(strFilter) == std::string::npos) continue;

        if (strPayment.find(',') != std::string::npos) {
            UniValue winner(UniValue::VARR);
            boost::char_separator<char> sep(",");
            boost::tokenizer< boost::char_separator<char> > tokens(strPayment, sep);
            for (const std::string& t : tokens) {
                UniValue addr(UniValue::VOBJ);
                std::size_t pos = t.find(":");
                std::string strAddress = t.substr(0,pos);
                uint64_t nVotes = atoi(t.substr(pos+1));
                addr.pushKV("address", strAddress);
                addr.pushKV("nVotes", nVotes);
                winner.push_back(addr);
            }
            obj.pushKV("winner", winner);
        } else if (strPayment.find("Unknown") == std::string::npos) {
            UniValue winner(UniValue::VOBJ);
            std::size_t pos = strPayment.find(":");
            std::string strAddress = strPayment.substr(0,pos);
            uint64_t nVotes = atoi(strPayment.substr(pos+1));
            winner.pushKV("address", strAddress);
            winner.pushKV("nVotes", nVotes);
            obj.pushKV("winner", winner);
        } else {
            UniValue winner(UniValue::VOBJ);
            winner.pushKV("address", strPayment);
            winner.pushKV("nVotes", 0);
            obj.pushKV("winner", winner);
        }

            ret.push_back(obj);
    }

    return ret;
}

UniValue getgamemasterscores(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getgamemasterscores ( blocks )\n"
            "\nPrint list of winning gamemaster by score\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Show the last n blocks (default 10)\n"

            "\nResult:\n"
            "{\n"
            "  xxxx: \"xxxx\"   (numeric : string) Block height : Gamemaster hash\n"
            "  ,...\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getgamemasterscores", "") + HelpExampleRpc("getgamemasterscores", ""));

    int nLast = 10;

    if (request.params.size() == 1) {
        try {
            nLast = std::stoi(request.params[0].get_str());
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("Exception on param 2");
        }
    }

    std::vector<std::pair<GamemasterRef, int>> vGmScores = gamemasterman.GetGmScores(nLast);
    if (vGmScores.empty()) return "unknown";

    UniValue obj(UniValue::VOBJ);
    for (const auto& p : vGmScores) {
        const GamemasterRef& gm = p.first;
        const int nHeight = p.second;
        obj.pushKV(strprintf("%d", nHeight), gm->vin.prevout.hash.ToString().c_str());
    }
    return obj;
}

bool DecodeHexGmb(CGamemasterBroadcast& gmb, std::string strHexGmb) {

    if (!IsHex(strHexGmb))
        return false;

    std::vector<unsigned char> gmbData(ParseHex(strHexGmb));
    CDataStream ssData(gmbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> gmb;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}
UniValue creategamemasterbroadcast(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    std::string strCommand;
    if (request.params.size() >= 1)
        strCommand = request.params[0].get_str();
    if (request.fHelp || (strCommand != "alias" && strCommand != "all") || (strCommand == "alias" && request.params.size() < 2))
        throw std::runtime_error(
            "creategamemasterbroadcast \"command\" ( \"alias\")\n"
            "\nCreates a gamemaster broadcast message for one or all gamemasters configured in gamemaster.conf\n" +
            HelpRequiringPassphrase(pwallet) + "\n"

            "\nArguments:\n"
            "1. \"command\"      (string, required) \"alias\" for single gamemaster, \"all\" for all gamemasters\n"
            "2. \"alias\"        (string, required if command is \"alias\") Alias of the gamemaster\n"

            "\nResult (all):\n"
            "{\n"
            "  \"overall\": \"xxx\",        (string) Overall status message indicating number of successes.\n"
            "  \"detail\": [                (array) JSON array of broadcast objects.\n"
            "    {\n"
            "      \"alias\": \"xxx\",      (string) Alias of the gamemaster.\n"
            "      \"success\": true|false, (boolean) Success status.\n"
            "      \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "      \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nResult (alias):\n"
            "{\n"
            "  \"alias\": \"xxx\",      (string) Alias of the gamemaster.\n"
            "  \"success\": true|false, (boolean) Success status.\n"
            "  \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "  \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("creategamemasterbroadcast", "alias mygm1") + HelpExampleRpc("creategamemasterbroadcast", "alias mygm1"));

    EnsureWalletIsUnlocked(pwallet);

    if (strCommand == "alias")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        std::string alias = request.params[1].get_str();
        bool found = false;

        UniValue statusObj(UniValue::VOBJ);
        statusObj.pushKV("alias", alias);

        for (CGamemasterConfig::CGamemasterEntry gme : gamemasterConfig.getEntries()) {
            if(gme.getAlias() == alias) {
                CGamemasterBroadcast gmb;
                found = true;
                std::string errorMessage;
                bool fSuccess = false;
                if (!StartGamemasterEntry(statusObj, gmb, fSuccess, gme, errorMessage, strCommand))
                        continue;
                SerializeGMB(statusObj, gmb, fSuccess);
                break;
            }
        }

        if(!found) {
            statusObj.pushKV("success", false);
            statusObj.pushKV("error_message", "Could not find alias in config. Verify with listgamemasterconf.");
        }

        return statusObj;
    }

    if (strCommand == "all")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        for (CGamemasterConfig::CGamemasterEntry gme : gamemasterConfig.getEntries()) {
            UniValue statusObj(UniValue::VOBJ);
            CGamemasterBroadcast gmb;
            std::string errorMessage;
            bool fSuccess = false;
            if (!StartGamemasterEntry(statusObj, gmb, fSuccess, gme, errorMessage, strCommand))
                    continue;
            SerializeGMB(statusObj, gmb, fSuccess, successful, failed);
            resultsObj.push_back(statusObj);
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.pushKV("overall", strprintf("Successfully created broadcast messages for %d gamemasters, failed to create %d, total %d", successful, failed, successful + failed));
        returnObj.pushKV("detail", resultsObj);

        return returnObj;
    }
    return NullUniValue;
}

UniValue decodegamemasterbroadcast(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "decodegamemasterbroadcast \"hexstring\"\n"
            "\nCommand to decode gamemaster broadcast messages\n"

            "\nArgument:\n"
            "1. \"hexstring\"        (string) The hex encoded gamemaster broadcast message\n"

            "\nResult:\n"
            "{\n"
            "  \"vin\": \"xxxx\"                (string) The unspent output which is holding the gamemaster collateral\n"
            "  \"addr\": \"xxxx\"               (string) IP address of the gamemaster\n"
            "  \"pubkeycollateral\": \"xxxx\"   (string) Collateral address's public key\n"
            "  \"pubkeygamemaster\": \"xxxx\"   (string) Gamemaster's public key\n"
            "  \"vchsig\": \"xxxx\"             (string) Base64-encoded signature of this message (verifiable via pubkeycollateral)\n"
            "  \"sigtime\": \"nnn\"             (numeric) Signature timestamp\n"
            "  \"sigvalid\": \"xxx\"            (string) \"true\"/\"false\" whether or not the gmb signature checks out.\n"
            "  \"protocolversion\": \"nnn\"     (numeric) Gamemaster's protocol version\n"
            "  \"nMessVersion\": \"nnn\"        (numeric) GMB Message version number\n"
            "  \"lastping\" : {                 (object) JSON object with information about the gamemaster's last ping\n"
            "      \"vin\": \"xxxx\"            (string) The unspent output of the gamemaster which is signing the message\n"
            "      \"blockhash\": \"xxxx\"      (string) Current chaintip blockhash minus 12\n"
            "      \"sigtime\": \"nnn\"         (numeric) Signature time for this ping\n"
            "      \"sigvalid\": \"xxx\"        (string) \"true\"/\"false\" whether or not the gmp signature checks out.\n"
            "      \"vchsig\": \"xxxx\"         (string) Base64-encoded signature of this ping (verifiable via pubkeygamemaster)\n"
            "      \"nMessVersion\": \"nnn\"    (numeric) GMP Message version number\n"
            "  }\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decodegamemasterbroadcast", "hexstring") + HelpExampleRpc("decodegamemasterbroadcast", "hexstring"));

    CGamemasterBroadcast gmb;

    if (!DecodeHexGmb(gmb, request.params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Gamemaster broadcast message decode failed");

    UniValue resultObj(UniValue::VOBJ);

    resultObj.pushKV("vin", gmb.vin.prevout.ToString());
    resultObj.pushKV("addr", gmb.addr.ToString());
    resultObj.pushKV("pubkeycollateral", EncodeDestination(gmb.pubKeyCollateralAddress.GetID()));
    resultObj.pushKV("pubkeygamemaster", EncodeDestination(gmb.pubKeyGamemaster.GetID()));
    resultObj.pushKV("vchsig", gmb.GetSignatureBase64());
    resultObj.pushKV("sigtime", gmb.sigTime);
    resultObj.pushKV("sigvalid", gmb.CheckSignature() ? "true" : "false");
    resultObj.pushKV("protocolversion", gmb.protocolVersion);
    resultObj.pushKV("nMessVersion", gmb.nMessVersion);

    UniValue lastPingObj(UniValue::VOBJ);
    lastPingObj.pushKV("vin", gmb.lastPing.vin.prevout.ToString());
    lastPingObj.pushKV("blockhash", gmb.lastPing.blockHash.ToString());
    lastPingObj.pushKV("sigtime", gmb.lastPing.sigTime);
    lastPingObj.pushKV("sigvalid", gmb.lastPing.CheckSignature(gmb.pubKeyGamemaster.GetID()) ? "true" : "false");
    lastPingObj.pushKV("vchsig", gmb.lastPing.GetSignatureBase64());
    lastPingObj.pushKV("nMessVersion", gmb.lastPing.nMessVersion);

    resultObj.pushKV("lastping", lastPingObj);

    return resultObj;
}

UniValue relaygamemasterbroadcast(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "relaygamemasterbroadcast \"hexstring\"\n"
            "\nCommand to relay gamemaster broadcast messages\n"

            "\nArguments:\n"
            "1. \"hexstring\"        (string) The hex encoded gamemaster broadcast message\n"

            "\nExamples:\n" +
            HelpExampleCli("relaygamemasterbroadcast", "hexstring") + HelpExampleRpc("relaygamemasterbroadcast", "hexstring"));


    CGamemasterBroadcast gmb;

    if (!DecodeHexGmb(gmb, request.params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Gamemaster broadcast message decode failed");

    if(!gmb.CheckSignature())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Gamemaster broadcast signature verification failed");

    gamemasterman.UpdateGamemasterList(gmb);
    gmb.Relay();

    return strprintf("Gamemaster broadcast sent (service %s, vin %s)", gmb.addr.ToString(), gmb.vin.ToString());
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                         actor (function)            okSafe argNames
  //  --------------------- ---------------------------  --------------------------  ------ --------
    { "gamemaster",         "creategamemasterbroadcast", &creategamemasterbroadcast, true,  {"command","alias"} },
    { "gamemaster",         "creategamemasterkey",       &creategamemasterkey,       true,  {} },
    { "gamemaster",         "decodegamemasterbroadcast", &decodegamemasterbroadcast, true,  {"hexstring"} },
    { "gamemaster",         "getgamemastercount",        &getgamemastercount,        true,  {} },
    { "gamemaster",         "getgamemasteroutputs",      &getgamemasteroutputs,      true,  {} },
    { "gamemaster",         "getgamemasterscores",       &getgamemasterscores,       true,  {"blocks"} },
    { "gamemaster",         "getgamemasterstatus",       &getgamemasterstatus,       true,  {} },
    { "gamemaster",         "getgamemasterwinners",      &getgamemasterwinners,      true,  {"blocks","filter"} },
    { "gamemaster",         "initgamemaster",            &initgamemaster,            true,  {"privkey","address","deterministic"} },
    { "gamemaster",         "listgamemasterconf",        &listgamemasterconf,        true,  {"filter"} },
    { "gamemaster",         "listgamemasters",           &listgamemasters,           true,  {"filter"} },
    { "gamemaster",         "gamemastercurrent",         &gamemastercurrent,         true,  {} },
    { "gamemaster",         "relaygamemasterbroadcast",  &relaygamemasterbroadcast,  true,  {"hexstring"} },
    { "gamemaster",         "startgamemaster",           &startgamemaster,           true,  {"set","lock_wallet","alias","reload_conf"} },

    /** Not shown in help */
    { "hidden",             "getcachedblockhashes",      &getcachedblockhashes,      true,  {} },
    { "hidden",             "gmping",                    &gmping,                    true,  {} },
};
// clang-format on

void RegisterGamemasterRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
