// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"

#include "base58.h"
#include "init.h"
#include "validation.h"
#include "random.h"
#include "sync.h"
#include "guiinterface.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_upper()

#include <univalue.h>


static bool fRPCRunning = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static CCriticalSection cs_rpcWarmup;

/* Timer-creating functions */
static RPCTimerInterface* timerInterface = NULL;
/* Map of name to timer.
 * @note Can be changed to std::unique_ptr when C++11 */
static std::map<std::string, boost::shared_ptr<RPCTimerBase> > deadlineTimers;

static struct CRPCSignals
{
    boost::signals2::signal<void ()> Started;
    boost::signals2::signal<void ()> Stopped;
    boost::signals2::signal<void (const CRPCCommand&)> PreCommand;
    boost::signals2::signal<void (const CRPCCommand&)> PostCommand;
} g_rpcSignals;

void RPCServer::OnStarted(boost::function<void ()> slot)
{
    g_rpcSignals.Started.connect(slot);
}

void RPCServer::OnStopped(boost::function<void ()> slot)
{
    g_rpcSignals.Stopped.connect(slot);
}

void RPCServer::OnPreCommand(boost::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PreCommand.connect(boost::bind(slot, _1));
}

void RPCServer::OnPostCommand(boost::function<void (const CRPCCommand&)> slot)
{
    g_rpcSignals.PostCommand.connect(boost::bind(slot, _1));
}

void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    for (UniValue::VType t : typesExpected) {
        if (params.size() <= i)
            break;

        const UniValue& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.isNull())))) {
            std::string err = strprintf("Expected type %s, got %s",
                                   uvTypeName(t), uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected,
                  bool fAllowNull)
{
    for (const PAIRTYPE(std::string, UniValue::VType)& t : typesExpected) {
        const UniValue& v = find_value(o, t.first);
        if (!fAllowNull && v.isNull())
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.isNull())))) {
            std::string err = strprintf("Expected type %s for %s, got %s",
                                   uvTypeName(t.second), t.first, uvTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

static inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

CAmount AmountFromValue(const UniValue& value)
{
    if (!value.isNum())
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount is not a number");

    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > 21000000.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    CAmount nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

UniValue ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return UniValue(UniValue::VNUM,
            strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder));
}

uint256 ParseHashV(const UniValue& v, std::string strName)
{
    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be hexadecimal string (not '" + strHex + "')");
    if (64 != strHex.length())
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("%s must be of length %d (not %d)", strName, 64, strHex.length()));
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const UniValue& o, std::string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName)
{
    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be hexadecimal string (not '" + strHex + "')");
    return ParseHex(strHex);
}
std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}

int ParseInt(const UniValue& o, std::string strKey)
{
    const UniValue& v = find_value(o, strKey);
    if (v.isNum())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, " + strKey + "is not an int");

    return v.get_int();
}

bool ParseBool(const UniValue& o, std::string strKey)
{
    const UniValue& v = find_value(o, strKey);
    if (v.isBool())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, " + strKey + "is not a bool");

    return v.get_bool();
}


/**
 * Note: This interface may still be subject to change.
 */

std::string CRPCTable::help(std::string strCommand) const
{
    std::string strRet;
    std::string category;
    std::set<rpcfn_type> setDone;
    std::vector<std::pair<std::string, const CRPCCommand*> > vCommands;

    for (std::map<std::string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
        vCommands.push_back(std::make_pair(mi->second->category + mi->first, mi->second));
    std::sort(vCommands.begin(), vCommands.end());

    for (const PAIRTYPE(std::string, const CRPCCommand*) & command : vCommands) {
        const CRPCCommand* pcmd = command.second;
        std::string strMethod = pcmd->name;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != std::string::npos)
            continue;
        if ((strCommand != "" || pcmd->category == "hidden") && strMethod != strCommand)
            continue;
#ifdef ENABLE_WALLET
        if (pcmd->reqWallet && !pwalletMain)
            continue;
#endif

        try {
            UniValue params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        } catch (std::exception& e) {
            // Help text is returned in an exception
            std::string strHelp = std::string(e.what());
            if (strCommand == "") {
                if (strHelp.find('\n') != std::string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));

                if (category != pcmd->category) {
                    if (!category.empty())
                        strRet += "\n";
                    category = pcmd->category;
                    std::string firstLetter = category.substr(0, 1);
                    boost::to_upper(firstLetter);
                    strRet += "== " + firstLetter + category.substr(1) + " ==\n";
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0, strRet.size() - 1);
    return strRet;
}

UniValue help(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "help ( \"command\" )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"     (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"     (string) The help text\n");

    std::string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


UniValue stop(const UniValue& params, bool fHelp)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "stop\n"
            "\nStop PIVX server.");
    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    StartShutdown();
    return "PIVX server stopping";
}


/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
    {
        //  category              name                      actor (function)         okSafeMode threadSafe reqWallet
        //  --------------------- ------------------------  -----------------------  ---------- ---------- ---------
        /* Overall control/query calls */
        {"control", "getinfo", &getinfo, true, false, false}, /* uses wallet if enabled */
        {"control", "help", &help, true, true, false},
        {"control", "stop", &stop, true, true, false},

        /* P2P networking */
        {"network", "getnetworkinfo", &getnetworkinfo, true, false, false},
        {"network", "addnode", &addnode, true, true, false},
        {"network", "disconnectnode", &disconnectnode, true, true, false},
        {"network", "getaddednodeinfo", &getaddednodeinfo, true, true, false},
        {"network", "getconnectioncount", &getconnectioncount, true, false, false},
        {"network", "getnettotals", &getnettotals, true, true, false},
        {"network", "getpeerinfo", &getpeerinfo, true, false, false},
        {"network", "ping", &ping, true, false, false},
        {"network", "setban", &setban, true, false, false},
        {"network", "listbanned", &listbanned, true, false, false},
        {"network", "clearbanned", &clearbanned, true, false, false},

        /* Block chain and UTXO */
        {"blockchain", "findserial", &findserial, true, false, false},
        {"blockchain", "getaccumulatorvalues", &getaccumulatorvalues, true, false, false},
        {"blockchain", "getaccumulatorwitness", &getaccumulatorwitness, true, false, false},
        {"blockchain", "getblockindexstats", &getblockindexstats, true, false, false},
        {"blockchain", "getmintsinblocks", &getmintsinblocks, true, false, false},
        {"blockchain", "getserials", &getserials, true, false, false},
        {"blockchain", "getblockchaininfo", &getblockchaininfo, true, false, false},
        {"blockchain", "getbestblockhash", &getbestblockhash, true, false, false},
        {"blockchain", "getblockcount", &getblockcount, true, false, false},
        {"blockchain", "getblock", &getblock, true, false, false},
        {"blockchain", "getblockhash", &getblockhash, true, false, false},
        {"blockchain", "getblockheader", &getblockheader, false, false, false},
        {"blockchain", "getchaintips", &getchaintips, true, false, false},
        {"blockchain", "getchecksumblock", &getchecksumblock, false, false, false},
        {"blockchain", "getdifficulty", &getdifficulty, true, false, false},
        {"blockchain", "getfeeinfo", &getfeeinfo, true, false, false},
        {"blockchain", "getmempoolinfo", &getmempoolinfo, true, true, false},
        {"blockchain", "getrawmempool", &getrawmempool, true, false, false},
        {"blockchain", "gettxout", &gettxout, true, false, false},
        {"blockchain", "gettxoutsetinfo", &gettxoutsetinfo, true, false, false},
        {"blockchain", "invalidateblock", &invalidateblock, true, true, false},
        {"blockchain", "reconsiderblock", &reconsiderblock, true, true, false},
        {"blockchain", "verifychain", &verifychain, true, false, false},

        /* Mining */
        {"mining", "getblocktemplate", &getblocktemplate, true, false, false},
        {"mining", "getmininginfo", &getmininginfo, true, false, false},
        {"mining", "getnetworkhashps", &getnetworkhashps, true, false, false},
        {"mining", "prioritisetransaction", &prioritisetransaction, true, false, false},
        {"mining", "submitblock", &submitblock, true, true, false},
        {"mining", "reservebalance", &reservebalance, true, true, false},

#ifdef ENABLE_WALLET
        /* Coin generation */
        {"generating", "getgenerate", &getgenerate, true, false, false},
        {"generating", "gethashespersec", &gethashespersec, true, false, false},
        {"generating", "setgenerate", &setgenerate, true, true, false},
        {"generating", "generate", &generate, true, true, false},
#endif

        /* Raw transactions */
        {"rawtransactions", "createrawtransaction", &createrawtransaction, true, false, false},
        {"rawtransactions", "decoderawtransaction", &decoderawtransaction, true, false, false},
        {"rawtransactions", "decodescript", &decodescript, true, false, false},
        {"rawtransactions", "getrawtransaction", &getrawtransaction, true, false, false},
        {"rawtransactions", "sendrawtransaction", &sendrawtransaction, false, false, false},
        {"rawtransactions", "signrawtransaction", &signrawtransaction, false, false, false}, /* uses wallet if enabled */

        /* Utility functions */
        {"util", "createmultisig", &createmultisig, true, true, false},
        {"util", "validateaddress", &validateaddress, true, false, false}, /* uses wallet if enabled */
        {"util", "verifymessage", &verifymessage, true, false, false},
        {"util", "estimatefee", &estimatefee, true, true, false},
        {"util", "estimatepriority", &estimatepriority, true, true, false},

        /* Not shown in help */
        {"hidden", "invalidateblock", &invalidateblock, true, true, false},
        {"hidden", "reconsiderblock", &reconsiderblock, true, true, false},
        {"hidden", "setmocktime", &setmocktime, true, false, false},
        { "hidden",             "waitfornewblock",        &waitfornewblock,        true,  true,  false  },
        { "hidden",             "waitforblock",           &waitforblock,           true,  true,  false  },
        { "hidden",             "waitforblockheight",     &waitforblockheight,     true,  true,  false  },

        /* PIVX features */
        {"pivx", "listmasternodes", &listmasternodes, true, true, false},
        {"pivx", "getmasternodecount", &getmasternodecount, true, true, false},
        {"pivx", "masternodeconnect", &masternodeconnect, true, true, false},
        {"pivx", "createmasternodebroadcast", &createmasternodebroadcast, true, true, false},
        {"pivx", "decodemasternodebroadcast", &decodemasternodebroadcast, true, true, false},
        {"pivx", "relaymasternodebroadcast", &relaymasternodebroadcast, true, true, false},
        {"pivx", "masternodecurrent", &masternodecurrent, true, true, false},
        {"pivx", "masternodedebug", &masternodedebug, true, true, false},
        {"pivx", "startmasternode", &startmasternode, true, true, false},
        {"pivx", "createmasternodekey", &createmasternodekey, true, true, false},
        {"pivx", "getmasternodeoutputs", &getmasternodeoutputs, true, true, false},
        {"pivx", "listmasternodeconf", &listmasternodeconf, true, true, false},
        {"pivx", "getmasternodestatus", &getmasternodestatus, true, true, false},
        {"pivx", "getmasternodewinners", &getmasternodewinners, true, true, false},
        {"pivx", "getmasternodescores", &getmasternodescores, true, true, false},
        {"pivx", "preparebudget", &preparebudget, true, true, false},
        {"pivx", "submitbudget", &submitbudget, true, true, false},
        {"pivx", "mnbudgetvote", &mnbudgetvote, true, true, false},
        {"pivx", "getbudgetvotes", &getbudgetvotes, true, true, false},
        {"pivx", "getnextsuperblock", &getnextsuperblock, true, true, false},
        {"pivx", "getbudgetprojection", &getbudgetprojection, true, true, false},
        {"pivx", "getbudgetinfo", &getbudgetinfo, true, true, false},
        {"pivx", "mnbudgetrawvote", &mnbudgetrawvote, true, true, false},
        {"pivx", "mnfinalbudget", &mnfinalbudget, true, true, false},
        {"pivx", "checkbudgets", &checkbudgets, true, true, false},
        {"pivx", "mnsync", &mnsync, true, true, false},
        {"pivx", "spork", &spork, true, true, false},
        {"pivx", "getpoolinfo", &getpoolinfo, true, true, false},

#ifdef ENABLE_WALLET
        /* Wallet */
        {"wallet", "addmultisigaddress", &addmultisigaddress, true, false, true},
        {"wallet", "autocombinerewards", &autocombinerewards, false, false, true},
        {"wallet", "backupwallet", &backupwallet, true, false, true},
        {"wallet", "enableautomintaddress", &enableautomintaddress, true, false, true},
        {"wallet", "createautomintaddress", &createautomintaddress, true, false, true},
        {"wallet", "dumpprivkey", &dumpprivkey, true, false, true},
        {"wallet", "dumpwallet", &dumpwallet, true, false, true},
        {"wallet", "bip38encrypt", &bip38encrypt, true, false, true},
        {"wallet", "bip38decrypt", &bip38decrypt, true, false, true},
        {"wallet", "encryptwallet", &encryptwallet, true, false, true},
        {"wallet", "getaccountaddress", &getaccountaddress, true, false, true},
        {"wallet", "getaccount", &getaccount, true, false, true},
        {"wallet", "getaddressesbyaccount", &getaddressesbyaccount, true, false, true},
        {"wallet", "getbalance", &getbalance, false, false, true},
        {"wallet", "getnewaddress", &getnewaddress, true, false, true},
        {"wallet", "getrawchangeaddress", &getrawchangeaddress, true, false, true},
        {"wallet", "getreceivedbyaccount", &getreceivedbyaccount, false, false, true},
        {"wallet", "getreceivedbyaddress", &getreceivedbyaddress, false, false, true},
        {"wallet", "getstakingstatus", &getstakingstatus, false, false, true},
        {"wallet", "getstakesplitthreshold", &getstakesplitthreshold, false, false, true},
        {"wallet", "gettransaction", &gettransaction, false, false, true},
        {"wallet", "getunconfirmedbalance", &getunconfirmedbalance, false, false, true},
        {"wallet", "getwalletinfo", &getwalletinfo, false, false, true},
        {"wallet", "importprivkey", &importprivkey, true, false, true},
        {"wallet", "importwallet", &importwallet, true, false, true},
        {"wallet", "importaddress", &importaddress, true, false, true},
        {"wallet", "keypoolrefill", &keypoolrefill, true, false, true},
        {"wallet", "listaccounts", &listaccounts, false, false, true},
        {"wallet", "listaddressgroupings", &listaddressgroupings, false, false, true},
        {"wallet", "listlockunspent", &listlockunspent, false, false, true},
        {"wallet", "listreceivedbyaccount", &listreceivedbyaccount, false, false, true},
        {"wallet", "listreceivedbyaddress", &listreceivedbyaddress, false, false, true},
        {"wallet", "listsinceblock", &listsinceblock, false, false, true},
        {"wallet", "listtransactions", &listtransactions, false, false, true},
        {"wallet", "listunspent", &listunspent, false, false, true},
        {"wallet", "lockunspent", &lockunspent, true, false, true},
        {"wallet", "move", &movecmd, false, false, true},
        {"wallet", "multisend", &multisend, false, false, true},
        {"wallet", "sendfrom", &sendfrom, false, false, true},
        {"wallet", "sendmany", &sendmany, false, false, true},
        {"wallet", "sendtoaddress", &sendtoaddress, false, false, true},
        {"wallet", "sendtoaddressix", &sendtoaddressix, false, false, true},
        {"wallet", "setaccount", &setaccount, true, false, true},
        {"wallet", "setstakesplitthreshold", &setstakesplitthreshold, false, false, true},
        {"wallet", "settxfee", &settxfee, true, false, true},
        {"wallet", "signmessage", &signmessage, true, false, true},
        {"wallet", "walletlock", &walletlock, true, false, true},
        {"wallet", "walletpassphrasechange", &walletpassphrasechange, true, false, true},
        {"wallet", "walletpassphrase", &walletpassphrase, true, false, true},

        {"zerocoin", "createrawzerocoinstake", &createrawzerocoinstake, false, false, true},
        {"zerocoin", "createrawzerocoinpublicspend", &createrawzerocoinpublicspend, false, false, true},
        {"zerocoin", "getzerocoinbalance", &getzerocoinbalance, false, false, true},
        {"zerocoin", "listmintedzerocoins", &listmintedzerocoins, false, false, true},
        {"zerocoin", "listspentzerocoins", &listspentzerocoins, false, false, true},
        {"zerocoin", "listzerocoinamounts", &listzerocoinamounts, false, false, true},
        {"zerocoin", "mintzerocoin", &mintzerocoin, false, false, true},
        {"zerocoin", "spendzerocoin", &spendzerocoin, false, false, true},
        {"zerocoin", "spendrawzerocoin", &spendrawzerocoin, true, false, false},
        {"zerocoin", "spendzerocoinmints", &spendzerocoinmints, false, false, true},
        {"zerocoin", "resetmintzerocoin", &resetmintzerocoin, false, false, true},
        {"zerocoin", "resetspentzerocoin", &resetspentzerocoin, false, false, true},
        {"zerocoin", "getarchivedzerocoin", &getarchivedzerocoin, false, false, true},
        {"zerocoin", "importzerocoins", &importzerocoins, false, false, true},
        {"zerocoin", "exportzerocoins", &exportzerocoins, false, false, true},
        {"zerocoin", "reconsiderzerocoins", &reconsiderzerocoins, false, false, true},
        {"zerocoin", "getspentzerocoinamount", &getspentzerocoinamount, false, false, false},
        {"zerocoin", "getzpivseed", &getzpivseed, false, false, true},
        {"zerocoin", "setzpivseed", &setzpivseed, false, false, true},
        {"zerocoin", "generatemintlist", &generatemintlist, false, false, true},
        {"zerocoin", "searchdzpiv", &searchdzpiv, false, false, true},
        {"zerocoin", "dzpivstate", &dzpivstate, false, false, true},
        {"zerocoin", "clearspendcache", &clearspendcache, false, false, true}

#endif // ENABLE_WALLET
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++) {
        const CRPCCommand* pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](const std::string &name) const
{
    std::map<std::string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

bool StartRPC()
{
    LogPrint("rpc", "Starting RPC\n");
    fRPCRunning = true;
    g_rpcSignals.Started();
    return true;
}

void InterruptRPC()
{
    LogPrint("rpc", "Interrupting RPC\n");
    // Interrupt e.g. running longpolls
    fRPCRunning = false;
}

void StopRPC()
{
    LogPrint("rpc", "Stopping RPC\n");
    deadlineTimers.clear();
    g_rpcSignals.Stopped();
}

bool IsRPCRunning()
{
    return fRPCRunning;
}

void SetRPCWarmupStatus(const std::string& newStatus)
{
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished()
{
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string* outStatus)
{
    LOCK(cs_rpcWarmup);
    if (outStatus)
        *outStatus = rpcWarmupStatus;
    return fRPCInWarmup;
}

void JSONRequest::parse(const UniValue& valRequest)
{
    // Parse request
    if (!valRequest.isObject())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const UniValue& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    UniValue valMethod = find_value(request, "method");
    if (valMethod.isNull())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (!valMethod.isStr())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getblocktemplate")
        LogPrint("rpc", "ThreadRPCServer method=%s\n", SanitizeString(strMethod));

    // Parse params
    UniValue valParams = find_value(request, "params");
    if (valParams.isArray())
        params = valParams.get_array();
    else if (valParams.isNull())
        params = UniValue(UniValue::VARR);
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}


static UniValue JSONRPCExecOne(const UniValue& req)
{
    UniValue rpc_result(UniValue::VOBJ);

    JSONRequest jreq;
    try {
        jreq.parse(req);

        UniValue result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, NullUniValue, jreq.id);
    } catch (const UniValue& objError) {
        rpc_result = JSONRPCReplyObj(NullUniValue, objError, jreq.id);
    } catch (std::exception& e) {
        rpc_result = JSONRPCReplyObj(NullUniValue,
            JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

std::string JSONRPCExecBatch(const UniValue& vReq)
{
    UniValue ret(UniValue::VARR);
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return ret.write() + "\n";
}

UniValue CRPCTable::execute(const std::string &strMethod, const UniValue &params) const
{
    // Find method
    const CRPCCommand* pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    g_rpcSignals.PreCommand(*pcmd);

    try {
        // Execute
        return pcmd->actor(params, false);
    } catch (std::exception& e) {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }

    g_rpcSignals.PostCommand(*pcmd);
}

std::vector<std::string> CRPCTable::listCommands() const
{
    std::vector<std::string> commandList;
    typedef std::map<std::string, const CRPCCommand*> commandMap;

    std::transform( mapCommands.begin(), mapCommands.end(),
                   std::back_inserter(commandList),
                   boost::bind(&commandMap::value_type::first,_1) );
    return commandList;
}

std::string HelpExampleCli(std::string methodname, std::string args)
{
    return "> pivx-cli " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(std::string methodname, std::string args)
{
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
           "\"method\": \"" +
           methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:51473/\n";
}

void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface)
{
    if (!timerInterface)
        timerInterface = iface;
}

void RPCSetTimerInterface(RPCTimerInterface *iface)
{
    timerInterface = iface;
}

void RPCUnsetTimerInterface(RPCTimerInterface *iface)
{
    if (timerInterface == iface)
        timerInterface = NULL;
}

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
{
    if (!timerInterface)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No timer handler registered for RPC");
    deadlineTimers.erase(name);
    LogPrint("rpc", "queue run of timer %s in %i seconds (using %s)\n", name, nSeconds, timerInterface->Name());
    deadlineTimers.insert(std::make_pair(name, boost::shared_ptr<RPCTimerBase>(timerInterface->NewTimer(func, nSeconds*1000))));
}

const CRPCTable tableRPC;
