// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
//Copyright (c) 2015-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include "amount.h"
#include "rpc/protocol.h"
#include "uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

class CRPCCommand;

namespace RPCServer
{
    void OnStarted(std::function<void ()> slot);
    void OnStopped(std::function<void ()> slot);
    void OnPreCommand(std::function<void (const CRPCCommand&)> slot);
    void OnPostCommand(std::function<void (const CRPCCommand&)> slot);
}

class CBlockIndex;
class CNetAddr;

class JSONRequest
{
public:
    UniValue id;
    std::string strMethod;
    UniValue params;

    JSONRequest() { id = NullUniValue; }
    void parse(const UniValue& valRequest);
};

/** Query whether RPC is running */
bool IsRPCRunning();

/**
 * Set the RPC warmup status.  When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& newStatus);
/* Mark warmup as done.  RPC calls will be processed from now on.  */
void SetRPCWarmupFinished();

/* returns the current warmup state.  */
bool RPCIsInWarmup(std::string* statusOut);

/**
 * Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValue::VType>& typesExpected, bool fAllowNull=false);

/**
 * Check for expected keys/value types in an Object.
 * Use like: RPCTypeCheckObj(object, boost::assign::map_list_of("name", str_type)("value", int_type));
 */
void RPCTypeCheckObj(const UniValue& o,
                  const std::map<std::string, UniValue::VType>& typesExpected, bool fAllowNull=false);

/** Opaque base class for timers returned by NewTimerFunc.
 * This provides no methods at the moment, but makes sure that delete
 * cleans up the whole state.
 */
class RPCTimerBase
{
public:
    virtual ~RPCTimerBase() {}
};

/**
* RPC timer "driver".
 */
class RPCTimerInterface
{
public:
    virtual ~RPCTimerInterface() {}
    /** Implementation name */
    virtual const char *Name() = 0;
    /** Factory function for timers.
     * RPC will call the function to create a timer that will call func in *millis* milliseconds.
     * @note As the RPC mechanism is backend-neutral, it can use different implementations of timers.
     * This is needed to cope with the case in which there is no HTTP server, but
     * only GUI RPC console, and to break the dependency of pcserver on httprpc.
     */
    virtual RPCTimerBase* NewTimer(std::function<void(void)>& func, int64_t millis) = 0;
};

/** Set factory function for timers */
void RPCSetTimerInterface(RPCTimerInterface *iface);
/** Set factory function for timers, but only if unset */
void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface);
/** Unset factory function for timers */
void RPCUnsetTimerInterface(RPCTimerInterface *iface);

/**
 * Run func nSeconds from now.
 * Overrides previous timer <name> (if any).
 */
void RPCRunLater(const std::string& name, std::function<void(void)> func, int64_t nSeconds);

typedef UniValue(*rpcfn_type)(const UniValue& params, bool fHelp);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
    bool threadSafe;
    bool reqWallet;
};

/**
 * Sombe RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;

public:
    CRPCTable();
    const CRPCCommand* operator[](const std::string& name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   UniValue Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (UniValue) when an error happens.
     */
    UniValue execute(const std::string &method, const UniValue &params) const;

    /**
    * Returns a list of registered commands
    * @returns List of registered commands.
    */
    std::vector<std::string> listCommands() const;
};

extern const CRPCTable tableRPC;

/**
 * Utilities: convert hex-encoded Values
 * (throws error if not hex).
 */
extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey);
extern int ParseInt(const UniValue& o, std::string strKey);
extern bool ParseBool(const UniValue& o, std::string strKey);

extern int64_t nWalletUnlockTime;
extern CAmount AmountFromValue(const UniValue& value);
extern UniValue ValueFromAmount(const CAmount& amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);
extern std::string HelpRequiringPassphrase();
extern std::string HelpExampleCli(std::string methodname, std::string args);
extern std::string HelpExampleRpc(std::string methodname, std::string args);

extern void EnsureWalletIsUnlocked(bool fAllowAnonOnly = false);
// Ensure the wallet's existence.
extern void EnsureWallet();

extern UniValue getconnectioncount(const UniValue& params, bool fHelp); // in rpc/net.cpp
extern UniValue getpeerinfo(const UniValue& params, bool fHelp);
extern UniValue ping(const UniValue& params, bool fHelp);
extern UniValue addnode(const UniValue& params, bool fHelp);
extern UniValue disconnectnode(const UniValue& params, bool fHelp);
extern UniValue getaddednodeinfo(const UniValue& params, bool fHelp);
extern UniValue getnettotals(const UniValue& params, bool fHelp);
extern UniValue setban(const UniValue& params, bool fHelp);
extern UniValue listbanned(const UniValue& params, bool fHelp);
extern UniValue clearbanned(const UniValue& params, bool fHelp);

extern UniValue dumpprivkey(const UniValue& params, bool fHelp); // in rpcdump.cpp
extern UniValue importprivkey(const UniValue& params, bool fHelp);
extern UniValue importaddress(const UniValue& params, bool fHelp);
extern UniValue dumpwallet(const UniValue& params, bool fHelp);
extern UniValue importwallet(const UniValue& params, bool fHelp);
extern UniValue bip38encrypt(const UniValue& params, bool fHelp);
extern UniValue bip38decrypt(const UniValue& params, bool fHelp);

extern UniValue getgenerate(const UniValue& params, bool fHelp); // in rpc/mining.cpp
extern UniValue setgenerate(const UniValue& params, bool fHelp);
extern UniValue generate(const UniValue& params, bool fHelp);
extern UniValue getnetworkhashps(const UniValue& params, bool fHelp);
extern UniValue gethashespersec(const UniValue& params, bool fHelp);
extern UniValue getmininginfo(const UniValue& params, bool fHelp);
extern UniValue prioritisetransaction(const UniValue& params, bool fHelp);
extern UniValue getblocktemplate(const UniValue& params, bool fHelp);
extern UniValue submitblock(const UniValue& params, bool fHelp);
extern UniValue estimatefee(const UniValue& params, bool fHelp);
extern UniValue estimatepriority(const UniValue& params, bool fHelp);

extern UniValue delegatestake(const UniValue& params, bool fHelp); // in rpcwallet.cpp
extern UniValue rawdelegatestake(const UniValue& params, bool fHelp);
extern UniValue delegatoradd(const UniValue& params, bool fHelp);
extern UniValue delegatorremove(const UniValue& params, bool fHelp);
extern UniValue upgradewallet(const UniValue& params, bool fHelp);
extern UniValue sethdseed(const UniValue& params, bool fHelp);
extern UniValue getaddressinfo(const UniValue& params, bool fHelp);
extern UniValue getnewaddress(const UniValue& params, bool fHelp);
extern UniValue getnewstakingaddress(const UniValue& params, bool fHelp);
extern UniValue getaccountaddress(const UniValue& params, bool fHelp);
extern UniValue getrawchangeaddress(const UniValue& params, bool fHelp);
extern UniValue setaccount(const UniValue& params, bool fHelp);
extern UniValue getaccount(const UniValue& params, bool fHelp);
extern UniValue getaddressesbyaccount(const UniValue& params, bool fHelp);
extern UniValue sendtoaddress(const UniValue& params, bool fHelp);
extern UniValue sendtoaddressix(const UniValue& params, bool fHelp);
extern UniValue signmessage(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue getreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue getbalance(const UniValue& params, bool fHelp);
extern UniValue getcoldstakingbalance(const UniValue& params, bool fHelp);
extern UniValue getdelegatedbalance(const UniValue& params, bool fHelp);
extern UniValue getunconfirmedbalance(const UniValue& params, bool fHelp);
extern UniValue movecmd(const UniValue& params, bool fHelp);
extern UniValue sendfrom(const UniValue& params, bool fHelp);
extern UniValue sendmany(const UniValue& params, bool fHelp);
extern UniValue addmultisigaddress(const UniValue& params, bool fHelp);
extern UniValue listcoldutxos(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaddress(const UniValue& params, bool fHelp);
extern UniValue listreceivedbyaccount(const UniValue& params, bool fHelp);
extern UniValue listtransactions(const UniValue& params, bool fHelp);
extern UniValue listaddressgroupings(const UniValue& params, bool fHelp);
extern UniValue listaccounts(const UniValue& params, bool fHelp);
extern UniValue listdelegators(const UniValue& params, bool fHelp);
extern UniValue liststakingaddresses(const UniValue& params, bool fHelp);
extern UniValue listsinceblock(const UniValue& params, bool fHelp);
extern UniValue gettransaction(const UniValue& params, bool fHelp);
extern UniValue abandontransaction(const UniValue& params, bool fHelp);
extern UniValue backupwallet(const UniValue& params, bool fHelp);
extern UniValue keypoolrefill(const UniValue& params, bool fHelp);
extern UniValue walletpassphrase(const UniValue& params, bool fHelp);
extern UniValue walletpassphrasechange(const UniValue& params, bool fHelp);
extern UniValue walletlock(const UniValue& params, bool fHelp);
extern UniValue encryptwallet(const UniValue& params, bool fHelp);
extern UniValue getwalletinfo(const UniValue& params, bool fHelp);
extern UniValue getblockchaininfo(const UniValue& params, bool fHelp);
extern UniValue getnetworkinfo(const UniValue& params, bool fHelp);
extern UniValue setstakesplitthreshold(const UniValue& params, bool fHelp);
extern UniValue getstakesplitthreshold(const UniValue& params, bool fHelp);
extern UniValue multisend(const UniValue& params, bool fHelp);
extern UniValue autocombinerewards(const UniValue& params, bool fHelp);

extern UniValue getrawtransaction(const UniValue& params, bool fHelp); // in rpc/rawtransaction.cpp
extern UniValue listunspent(const UniValue& params, bool fHelp);
extern UniValue lockunspent(const UniValue& params, bool fHelp);
extern UniValue listlockunspent(const UniValue& params, bool fHelp);
extern UniValue createrawtransaction(const UniValue& params, bool fHelp);
extern UniValue decoderawtransaction(const UniValue& params, bool fHelp);
extern UniValue decodescript(const UniValue& params, bool fHelp);
extern UniValue signrawtransaction(const UniValue& params, bool fHelp);
extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);

extern UniValue getblockcount(const UniValue& params, bool fHelp);
extern UniValue getbestblockhash(const UniValue& params, bool fHelp);
extern UniValue waitfornewblock(const UniValue& params, bool fHelp);
extern UniValue waitforblock(const UniValue& params, bool fHelp);
extern UniValue waitforblockheight(const UniValue& params, bool fHelp);
extern UniValue getdifficulty(const UniValue& params, bool fHelp);
extern UniValue settxfee(const UniValue& params, bool fHelp);
extern UniValue getmempoolinfo(const UniValue& params, bool fHelp);
extern UniValue getrawmempool(const UniValue& params, bool fHelp);
extern UniValue getblockhash(const UniValue& params, bool fHelp);
extern UniValue getblock(const UniValue& params, bool fHelp);
extern UniValue getblockheader(const UniValue& params, bool fHelp);
extern UniValue getfeeinfo(const UniValue& params, bool fHelp);
extern UniValue gettxoutsetinfo(const UniValue& params, bool fHelp);
extern UniValue gettxout(const UniValue& params, bool fHelp);
extern UniValue verifychain(const UniValue& params, bool fHelp);
extern UniValue getchaintips(const UniValue& params, bool fHelp);
extern UniValue invalidateblock(const UniValue& params, bool fHelp);
extern UniValue reconsiderblock(const UniValue& params, bool fHelp);
extern UniValue getblockindexstats(const UniValue& params, bool fHelp);
extern void validaterange(const UniValue& params, int& heightStart, int& heightEnd, int minHeightStart=1);

// in rpc/masternode.cpp
extern UniValue listmasternodes(const UniValue& params, bool fHelp);
extern UniValue getmasternodecount(const UniValue& params, bool fHelp);
extern UniValue createmasternodebroadcast(const UniValue& params, bool fHelp);
extern UniValue decodemasternodebroadcast(const UniValue& params, bool fHelp);
extern UniValue relaymasternodebroadcast(const UniValue& params, bool fHelp);
extern UniValue masternodeconnect(const UniValue& params, bool fHelp);
extern UniValue masternodecurrent(const UniValue& params, bool fHelp);
extern UniValue masternodedebug(const UniValue& params, bool fHelp);
extern UniValue startmasternode(const UniValue& params, bool fHelp);
extern UniValue createmasternodekey(const UniValue& params, bool fHelp);
extern UniValue getmasternodeoutputs(const UniValue& params, bool fHelp);
extern UniValue listmasternodeconf(const UniValue& params, bool fHelp);
extern UniValue getmasternodestatus(const UniValue& params, bool fHelp);
extern UniValue getmasternodewinners(const UniValue& params, bool fHelp);
extern UniValue getmasternodescores(const UniValue& params, bool fHelp);

extern UniValue preparebudget(const UniValue& params, bool fHelp); // in rpc/budget.cpp
extern UniValue submitbudget(const UniValue& params, bool fHelp);
extern UniValue mnbudgetvote(const UniValue& params, bool fHelp);
extern UniValue getbudgetvotes(const UniValue& params, bool fHelp);
extern UniValue getnextsuperblock(const UniValue& params, bool fHelp);
extern UniValue getbudgetprojection(const UniValue& params, bool fHelp);
extern UniValue getbudgetinfo(const UniValue& params, bool fHelp);
extern UniValue mnbudgetrawvote(const UniValue& params, bool fHelp);
extern UniValue mnfinalbudget(const UniValue& params, bool fHelp);
extern UniValue checkbudgets(const UniValue& params, bool fHelp);

extern UniValue getinfo(const UniValue& params, bool fHelp); // in rpc/misc.cpp
extern UniValue logging(const UniValue& params, bool fHelp);
extern UniValue mnsync(const UniValue& params, bool fHelp);
extern UniValue spork(const UniValue& params, bool fHelp);
extern UniValue validateaddress(const UniValue& params, bool fHelp);
extern UniValue createmultisig(const UniValue& params, bool fHelp);
extern UniValue verifymessage(const UniValue& params, bool fHelp);
extern UniValue setmocktime(const UniValue& params, bool fHelp);
extern UniValue getstakingstatus(const UniValue& params, bool fHelp);

bool StartRPC();
void InterruptRPC();
void StopRPC();
std::string JSONRPCExecBatch(const UniValue& vReq);
void RPCNotifyBlockChange(bool fInitialDownload, const CBlockIndex* pindex);

#endif // BITCOIN_RPCSERVER_H
