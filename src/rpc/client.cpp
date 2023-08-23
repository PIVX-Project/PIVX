// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/client.h"

#include <set>
#include <stdint.h>


class CRPCConvertParam
{
public:
    std::string methodName; //! method whose params want conversion
    int paramIdx;           //! 0-based idx of param to convert
    std::string paramName; //!< parameter name
};

// clang-format off
/**
 * Specifiy a (method, idx, name) here if the argument is a non-string RPC
 * argument and needs to be converted from JSON.
 *
 * @note Parameter indexes start from 0.
 */
static const CRPCConvertParam vRPCConvertParams[] = {
    { "addmultisigaddress", 0, "nrequired" },
    { "addmultisigaddress", 1, "keys" },
    { "addpeeraddress", 1, "port" },
    { "cleanbudget", 0, "try_sync" },
    { "createmultisig", 0, "nrequired" },
    { "createmultisig", 1, "keys" },
    { "createrawtransaction", 0, "inputs" },
    { "createrawtransaction", 1, "outputs" },
    { "createrawtransaction", 2, "locktime" },
    { "createrawgmfinalbudget", 1, "blockstart" },
    { "createrawgmfinalbudget", 2, "proposals" },
    { "delegatestake", 1, "amount" },
    { "delegatestake", 3, "ext_owner" },
    { "delegatestake", 4, "include_delegated" },
    { "delegatestake", 5, "from_shield" },
    { "estimatefee", 0, "nblocks" },
    { "estimatesmartfee", 0, "nblocks" },
    { "fundrawtransaction", 1, "options" },
    { "generate", 0, "nblocks" },
    { "generatetoaddress", 0, "nblocks" },
    { "getaddednodeinfo", 0, "dummy" },
    { "getbalance", 0, "minconf" },
    { "getbalance", 1, "include_watchonly" },
    { "getbalance", 2, "include_delegated" },
    { "getbalance", 3, "include_shield" },
    { "getblock", 1, "verbosity" },
    { "getblock", 1, "verbose" },
    { "getblockhash", 0, "height" },
    { "getblockheader", 1, "verbose" },
    { "getblockindexstats", 0, "height" },
    { "getblockindexstats", 1, "range" },
    { "getblocktemplate", 0, "template_request" },
    { "getfeeinfo", 0, "blocks" },
    { "getshieldbalance", 1, "minconf" },
    { "getshieldbalance", 2, "include_watchonly" },
    { "getminedcommitment", 0, "llmq_type" },
    { "getnetworkhashps", 0, "nblocks" },
    { "getnetworkhashps", 1, "height" },
    { "getnodeaddresses", 0, "count" },
    { "getquorummembers", 0, "llmq_type" },
    { "getrawmempool", 0, "verbose" },
    { "getrawtransaction", 1, "verbose" },
    { "getreceivedbyaddress", 1, "minconf" },
    { "getreceivedbylabel", 1, "minconf" },
    { "getsaplingnotescount", 0, "minconf" },
    { "getsupplyinfo", 0, "force_update" },
    { "gettransaction", 1, "include_watchonly" },
    { "gettxout", 1, "n" },
    { "gettxout", 2, "include_mempool" },
    { "importaddress", 2, "rescan" },
    { "importaddress", 3, "p2sh" },
    { "importmulti", 0, "requests" },
    { "importmulti", 1, "options" },
    { "importprivkey", 2, "rescan" },
    { "importprivkey", 3, "is_staking_address" },
    { "importpubkey", 2, "rescan" },
    { "importsaplingkey", 1, "rescan" },
    { "importsaplingkey", 2, "height" },
    { "importsaplingviewingkey", 1, "rescan" },
    { "importsaplingviewingkey", 2, "height" },
    { "initgamemaster", 2, "deterministic" },
    { "keypoolrefill", 0, "newsize" },
    { "listcoldutxos", 0, "not_whitelisted" },
    { "listdelegators", 0, "blacklist" },
    { "listreceivedbyaddress", 0, "minconf" },
    { "listreceivedbyaddress", 1, "include_empty" },
    { "listreceivedbyaddress", 2, "include_watchonly" },
    { "listreceivedbylabel", 0, "minconf" },
    { "listreceivedbylabel", 1, "include_empty" },
    { "listreceivedbylabel", 2, "include_watchonly" },
    { "listreceivedbyshieldaddress", 1, "minconf" },
    { "listshieldunspent", 0, "minconf" },
    { "listshieldunspent", 1, "maxconf" },
    { "listshieldunspent", 2, "include_watchonly" },
    { "listshieldunspent", 3, "addresses" },
    { "listsinceblock", 1, "target_confirmations" },
    { "listsinceblock", 2, "include_watchonly" },
    { "listtransactions", 1, "count" },
    { "listtransactions", 2, "from" },
    { "listtransactions", 3, "include_watchonly" },
    { "listtransactions", 4, "include_delegated" },
    { "listtransactions", 5, "include_cold" },
    { "listunspent", 0, "minconf" },
    { "listunspent", 1, "maxconf" },
    { "listunspent", 2, "addresses" },
    { "listunspent", 3, "watchonly_config" },
    { "listunspent", 4, "query_options" },
    { "listunspent", 5, "include_unsafe" },
    { "lockunspent", 0, "unlock" },
    { "lockunspent", 1, "transparent" },
    { "lockunspent", 2, "transactions" },
    { "logging", 0, "include" },
    { "logging", 1, "exclude" },
    { "gmbudgetvote", 4, "legacy" },
    { "gmbudgetrawvote", 1, "collat_vout" },
    { "gmbudgetrawvote", 4, "time" },
    { "preparebudget", 2, "npayments" },
    { "preparebudget", 3, "start" },
    { "preparebudget", 5, "montly_payment" },
    { "prioritisetransaction", 1, "fee_delta" },
    { "protx_register_prepare", 1, "collateralIndex" },
    { "quorumdkgsimerror", 1, "rate" },
    { "quorumdkgstatus", 0, "detail_level" },
    { "listquorums", 0, "count" },
    { "getquoruminfo", 0, "llmqType" },
    { "getquoruminfo", 2, "includeSkShare" },
    { "signsession", 0, "llmqType"},
    { "hasrecoverysignature", 0, "llmqType" },
    { "issessionconflicting", 0, "llmqType" },
    { "rawdelegatestake", 1, "amount" },
    { "rawdelegatestake", 3, "ext_owner" },
    { "rawdelegatestake", 4, "include_delegated" },
    { "rawdelegatestake", 5, "from_shield" },
    { "rawdelegatestake", 6, "force" },
    { "rawshieldsendmany", 1, "amounts" },
    { "rawshieldsendmany", 2, "minconf" },
    { "rawshieldsendmany", 3, "fee" },
    { "rescanblockchain", 0, "start_height"},
    { "rescanblockchain", 1, "stop_height"},
    { "sendmany", 1, "amounts" },
    { "sendmany", 2, "minconf" },
    { "sendmany", 4, "include_delegated" },
    { "sendmany", 5, "subtract_fee_from" },
    { "scantxoutset", 1, "scanobjects" },
    { "sendrawtransaction", 1, "allowhighfees" },
    { "sendtoaddress", 1, "amount" },
    { "sendtoaddress", 4, "subtract_fee" },
    { "setautocombinethreshold", 0, "enable" },
    { "setautocombinethreshold", 1, "threshold" },
    { "setautocombinethreshold", 2, "frequency"},
    { "setnetworkactive", 0, "active" },
    { "setban", 2, "bantime" },
    { "setban", 3, "absolute" },
    { "setgenerate", 0, "generate" },
    { "setgenerate", 1, "genproclimit" },
    { "sethdseed", 0, "newkeypool" },
    { "setmocktime", 0, "timestamp" },
    { "setstakesplitthreshold", 0, "value" },
    { "settxfee", 0, "amount" },
    { "shieldsendmany", 1, "amounts" },
    { "shieldsendmany", 2, "minconf" },
    { "shieldsendmany", 3, "fee" },
    { "shieldsendmany", 4, "subtract_fee_from" },
    { "signrawtransaction", 1, "prevtxs" },
    { "signrawtransaction", 2, "privkeys" },
    { "spork", 1, "value" },
    { "startgamemaster", 1, "lock_wallet" },
    { "startgamemaster", 3, "reload_conf" },
    { "submitbudget", 2, "npayments" },
    { "submitbudget", 3, "start" },
    { "submitbudget", 5, "montly_payment" },
    { "verifychain", 0, "nblocks" },
    { "waitforblock", 1, "timeout" },
    { "waitforblockheight", 0, "height" },
    { "waitforblockheight", 1, "timeout" },
    { "waitfornewblock", 0, "timeout" },
    { "walletpassphrase", 1, "timeout" },
    { "walletpassphrase", 2, "staking_only" },
    { "gmconnect", 1, "gm_list" },
    { "gmconnect", 2, "llmq_type" },
    // Echo with conversion (For testing only)
    { "echojson", 0, "arg0" },
    { "echojson", 1, "arg1" },
    { "echojson", 2, "arg2" },
    { "echojson", 3, "arg3" },
    { "echojson", 4, "arg4" },
    { "echojson", 5, "arg5" },
    { "echojson", 6, "arg6" },
    { "echojson", 7, "arg7" },
    { "echojson", 8, "arg8" },
    { "echojson", 9, "arg9" },
};
// clang-format on

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int>> members;
    std::set<std::pair<std::string, std::string>> membersByName;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx)
    {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
    bool convert(const std::string& method, const std::string& name)
    {
        return (membersByName.count(std::make_pair(method, name)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
        membersByName.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                            vRPCConvertParams[i].paramName));
    }
}

static CRPCConvertTable rpcCvtTable;

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (!jVal.read(std::string("[")+strVal+std::string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw std::runtime_error(std::string("Error parsing JSON:")+strVal);
    return jVal[0];
}

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        if (!rpcCvtTable.convert(strMethod, idx)) {
            // insert string value directly
            params.push_back(strVal);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.push_back(ParseNonRFCJSONValue(strVal));
        }
    }

    return params;
}

UniValue RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VOBJ);

    for (const std::string &s: strParams) {
        size_t pos = s.find('=');
        if (pos == std::string::npos) {
            throw(std::runtime_error("No '=' in named argument '"+s+"', this needs to be present for every argument (even if it is empty)"));
        }

        std::string name = s.substr(0, pos);
        std::string value = s.substr(pos+1);

        if (!rpcCvtTable.convert(strMethod, name)) {
            // insert string value directly
            params.pushKV(name, value);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.pushKV(name, ParseNonRFCJSONValue(value));
        }
    }

    return params;
}
