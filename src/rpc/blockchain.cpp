// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "checkpoints.h"
#include "clientversion.h"
#include "main.h"
#include "rpc/server.h"
#include "sync.h"
#include "txdb.h"
#include "util.h"
#include "utilmoneystr.h"
#include "accumulatormap.h"
#include "accumulators.h"

#include <stdint.h>
#include <univalue.h>

using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL) {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

UniValue blockheaderToJSON(const CBlockIndex* blockindex)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", blockindex->nVersion));
    result.push_back(Pair("merkleroot", blockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", (int64_t)blockindex->nTime));
    result.push_back(Pair("mediantime", (int64_t)blockindex->GetMedianTimePast()));
    result.push_back(Pair("nonce", (uint64_t)blockindex->nNonce));
    result.push_back(Pair("bits", strprintf("%08x", blockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));
    result.push_back(Pair("acc_checkpoint", blockindex->nAccumulatorCheckpoint.GetHex()));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    result.push_back(Pair("acc_checkpoint", block.nAccumulatorCheckpoint.GetHex()));
    UniValue txs(UniValue::VARR);
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        if (txDetails) {
            UniValue objTx(UniValue::VOBJ);
            TxToJSON(tx, uint256(0), objTx);
            txs.push_back(objTx);
        } else
            txs.push_back(tx.GetHash().GetHex());
    }
    result.push_back(Pair("tx", txs));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("mediantime", (int64_t)blockindex->GetMedianTimePast()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex* pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));

    result.push_back(Pair("moneysupply",ValueFromAmount(blockindex->nMoneySupply)));

    UniValue zsynxObj(UniValue::VOBJ);
    for (auto denom : libzerocoin::zerocoinDenomList) {
        zsynxObj.push_back(Pair(to_string(denom), ValueFromAmount(blockindex->mapZerocoinSupply.at(denom) * (denom*COIN))));
    }
    zsynxObj.push_back(Pair("total", ValueFromAmount(blockindex->GetZerocoinSupply())));
    result.push_back(Pair("zSYNXsupply", zsynxObj));

    return result;
}

UniValue getblockcount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "\nReturns the number of blocks in the longest block chain.\n"

            "\nResult:\n"
            "n    (numeric) The current block count\n"

            "\nExamples:\n" +
            HelpExampleCli("getblockcount", "") + HelpExampleRpc("getblockcount", ""));

    LOCK(cs_main);
    return chainActive.Height();
}

UniValue getbestblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest block chain.\n"

            "\nResult\n"
            "\"hex\"      (string) the block hash hex encoded\n"

            "\nExamples\n" +
            HelpExampleCli("getbestblockhash", "") + HelpExampleRpc("getbestblockhash", ""));

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

UniValue getdifficulty(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"

            "\nResult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"

            "\nExamples:\n" +
            HelpExampleCli("getdifficulty", "") + HelpExampleRpc("getdifficulty", ""));

    LOCK(cs_main);
    return GetDifficulty();
}


UniValue mempoolToJSON(bool fVerbose = false)
{
    if (fVerbose) {
        LOCK(mempool.cs);
        UniValue o(UniValue::VOBJ);
        BOOST_FOREACH (const PAIRTYPE(uint256, CTxMemPoolEntry) & entry, mempool.mapTx) {
            const uint256& hash = entry.first;
            const CTxMemPoolEntry& e = entry.second;
            UniValue info(UniValue::VOBJ);
            info.push_back(Pair("size", (int)e.GetTxSize()));
            info.push_back(Pair("fee", ValueFromAmount(e.GetFee())));
            info.push_back(Pair("time", e.GetTime()));
            info.push_back(Pair("height", (int)e.GetHeight()));
            info.push_back(Pair("startingpriority", e.GetPriority(e.GetHeight())));
            info.push_back(Pair("currentpriority", e.GetPriority(chainActive.Height())));
            const CTransaction& tx = e.GetTx();
            set<string> setDepends;
            BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                if (mempool.exists(txin.prevout.hash))
                    setDepends.insert(txin.prevout.hash.ToString());
            }

            UniValue depends(UniValue::VARR);
            BOOST_FOREACH(const string& dep, setDepends) {
                depends.push_back(dep);
            }

            info.push_back(Pair("depends", depends));
            o.push_back(Pair(hash.ToString(), info));
        }
        return o;
    } else {
        vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        UniValue a(UniValue::VARR);
        BOOST_FOREACH (const uint256& hash, vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

UniValue getrawmempool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawmempool ( verbose )\n"
            "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"

            "\nArguments:\n"
            "1. verbose           (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"

            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"

            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            "    \"size\" : n,             (numeric) transaction size in bytes\n"
            "    \"fee\" : n,              (numeric) transaction fee in syndicate\n"
            "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
            "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
            "    \"startingpriority\" : n, (numeric) priority when transaction entered pool\n"
            "    \"currentpriority\" : n,  (numeric) transaction priority now\n"
            "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",    (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "]\n"

            "\nExamples\n" +
            HelpExampleCli("getrawmempool", "true") + HelpExampleRpc("getrawmempool", "true"));

    LOCK(cs_main);

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = params[0].get_bool();

    return mempoolToJSON(fVerbose);
}

UniValue getblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash index\n"
            "\nReturns hash of block in best-block-chain at index provided.\n"

            "\nArguments:\n"
            "1. index         (numeric, required) The block index\n"

            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"

            "\nExamples:\n" +
            HelpExampleCli("getblockhash", "1000") + HelpExampleRpc("getblockhash", "1000"));

    LOCK(cs_main);

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    CBlockIndex* pblockindex = chainActive[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

UniValue getblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
            "If verbose is true, returns an Object with information about block <hash>.\n"

            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"

            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "  \"moneysupply\" : \"supply\"       (numeric) The money supply when this block was added to the blockchain\n"
            "  \"zSYNXsupply\" :\n"
            "  {\n"
            "     \"1\" : n,            (numeric) supply of 1 zSYNX denomination\n"
            "     \"5\" : n,            (numeric) supply of 5 zSYNX denomination\n"
            "     \"10\" : n,           (numeric) supply of 10 zSYNX denomination\n"
            "     \"50\" : n,           (numeric) supply of 50 zSYNX denomination\n"
            "     \"100\" : n,          (numeric) supply of 100 zSYNX denomination\n"
            "     \"500\" : n,          (numeric) supply of 500 zSYNX denomination\n"
            "     \"1000\" : n,         (numeric) supply of 1000 zSYNX denomination\n"
            "     \"5000\" : n,         (numeric) supply of 5000 zSYNX denomination\n"
            "     \"total\" : n,        (numeric) The total supply of all zSYNX denominations\n"
            "  }\n"
            "}\n"

            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"

            "\nExamples:\n" +
            HelpExampleCli("getblock", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\"") +
            HelpExampleRpc("getblock", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\""));

    LOCK(cs_main);

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (!ReadBlockFromDisk(block, pblockindex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockToJSON(block, pblockindex);
}

UniValue getblockheader(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblockheader \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for block 'hash' header.\n"
            "If verbose is true, returns an Object with information about block <hash> header.\n"

            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"

            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "}\n"

            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash' header.\n"

            "\nExamples:\n" +
            HelpExampleCli("getblockheader", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\"") +
            HelpExampleRpc("getblockheader", "\"00000000000fd08c2fb661d2fcb0d49abb3a91e5f27082ce64feed3b4dede2e2\""));

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << pblockindex->GetBlockHeader();
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockheaderToJSON(pblockindex);
}

UniValue gettxoutsetinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "\nReturns statistics about the unspent transaction output set.\n"
            "Note this call may take some time.\n"

            "\nResult:\n"
            "{\n"
            "  \"height\":n,     (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) The number of transactions\n"
            "  \"txouts\": n,            (numeric) The number of output transactions\n"
            "  \"bytes_serialized\": n,  (numeric) The serialized size\n"
            "  \"hash_serialized\": \"hash\",   (string) The serialized hash\n"
            "  \"total_amount\": x.xxx          (numeric) The total amount\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("gettxoutsetinfo", "") + HelpExampleRpc("gettxoutsetinfo", ""));

    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    CCoinsStats stats;
    FlushStateToDisk();
    if (pcoinsTip->GetStats(stats)) {
        ret.push_back(Pair("height", (int64_t)stats.nHeight));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", (int64_t)stats.nTransactions));
        ret.push_back(Pair("txouts", (int64_t)stats.nTransactionOutputs));
        ret.push_back(Pair("bytes_serialized", (int64_t)stats.nSerializedSize));
        ret.push_back(Pair("hash_serialized", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    }
    return ret;
}

UniValue gettxout(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout \"txid\" n ( includemempool )\n"
            "\nReturns details about an unspent transaction output.\n"

            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id\n"
            "2. n              (numeric, required) vout value\n"
            "3. includemempool  (boolean, optional) Whether to included the mem pool\n"

            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in btc\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of syndicate addresses\n"
            "     \"syndicateaddress\"   	 	(string) syndicate address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,            (numeric) The version\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n" +
            HelpExampleCli("listunspent", "") +
            "\nView the details\n" +
            HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n" +
            HelpExampleRpc("gettxout", "\"txid\", 1"));

    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    int n = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return NullUniValue;
        mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return NullUniValue;
    }
    if (n < 0 || (unsigned int)n >= coins.vout.size() || coins.vout[n].IsNull())
        return NullUniValue;

    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    CBlockIndex* pindex = it->second;
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.push_back(Pair("confirmations", 0));
    else
        ret.push_back(Pair("confirmations", pindex->nHeight - coins.nHeight + 1));
    ret.push_back(Pair("value", ValueFromAmount(coins.vout[n].nValue)));
    UniValue o(UniValue::VOBJ);
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("version", coins.nVersion));
    ret.push_back(Pair("coinbase", coins.fCoinBase));

    return ret;
}

UniValue verifychain(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "verifychain ( numblocks )\n"
            "\nVerifies blockchain database.\n"

            "\nArguments:\n"
            "1. numblocks    (numeric, optional, default=288, 0=all) The number of blocks to check.\n"

            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"

            "\nExamples:\n" +
            HelpExampleCli("verifychain", "") + HelpExampleRpc("verifychain", ""));

    LOCK(cs_main);

    int nCheckLevel = 4;
    int nCheckDepth = GetArg("-checkblocks", 288);
    if (params.size() > 0)
        nCheckDepth = params[0].get_int();

    fVerifyingBlocks = true;
    bool fVerified = CVerifyDB().VerifyDB(pcoinsTip, nCheckLevel, nCheckDepth);
    fVerifyingBlocks = false;

    return fVerified;
}

/** Implementation of IsSuperMajority with better feedback */
static UniValue SoftForkMajorityDesc(int minVersion, CBlockIndex* pindex, int nRequired)
{
    int nFound = 0;
    CBlockIndex* pstart = pindex;
    for (int i = 0; i < Params().ToCheckBlockUpgradeMajority() && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    UniValue rv(UniValue::VOBJ);
    rv.push_back(Pair("status", nFound >= nRequired));
    rv.push_back(Pair("found", nFound));
    rv.push_back(Pair("required", nRequired));
    rv.push_back(Pair("window", Params().ToCheckBlockUpgradeMajority()));
    return rv;
}
static UniValue SoftForkDesc(const std::string &name, int version, CBlockIndex* pindex)
{
    UniValue rv(UniValue::VOBJ);
    rv.push_back(Pair("id", name));
    rv.push_back(Pair("version", version));
    rv.push_back(Pair("enforce", SoftForkMajorityDesc(version, pindex, Params().EnforceBlockUpgradeMajority())));
    rv.push_back(Pair("reject", SoftForkMajorityDesc(version, pindex, Params().RejectBlockOutdatedMajority())));
    return rv;
}

UniValue getblockchaininfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding block chain processing.\n"

            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",        (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,         (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,        (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\", (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,     (numeric) the current difficulty\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"chainwork\": \"xxxx\"     (string) total amount of work in active chain, in hexadecimal\n"
            "  \"softforks\": [            (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",        (string) name of softfork\n"
            "        \"version\": xx,         (numeric) block version\n"
            "        \"enforce\": {           (object) progress toward enforcing the softfork rules for new-version blocks\n"
            "           \"status\": xx,       (boolean) true if threshold reached\n"
            "           \"found\": xx,        (numeric) number of blocks with the new version found\n"
            "           \"required\": xx,     (numeric) number of blocks required to trigger\n"
            "           \"window\": xx,       (numeric) maximum size of examined window of recent blocks\n"
            "        },\n"
            "        \"reject\": { ... }      (object) progress toward rejecting pre-softfork blocks (same fields as \"enforce\")\n"
            "     }, ...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getblockchaininfo", "") + HelpExampleRpc("getblockchaininfo", ""));

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("chain", Params().NetworkIDString()));
    obj.push_back(Pair("blocks", (int)chainActive.Height()));
    obj.push_back(Pair("headers", pindexBestHeader ? pindexBestHeader->nHeight : -1));
    obj.push_back(Pair("bestblockhash", chainActive.Tip()->GetBlockHash().GetHex()));
    obj.push_back(Pair("difficulty", (double)GetDifficulty()));
    obj.push_back(Pair("verificationprogress", Checkpoints::GuessVerificationProgress(chainActive.Tip())));
    obj.push_back(Pair("chainwork", chainActive.Tip()->nChainWork.GetHex()));
    CBlockIndex* tip = chainActive.Tip();
    UniValue softforks(UniValue::VARR);
    softforks.push_back(SoftForkDesc("bip65", 5, tip));
    obj.push_back(Pair("softforks",             softforks));
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight {
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
            return (a->nHeight > b->nHeight);

        return a < b;
    }
};

UniValue getchaintips(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"

            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"

            "\nExamples:\n" +
            HelpExampleCli("getchaintips", "") + HelpExampleRpc("getchaintips", ""));

    LOCK(cs_main);

    /* Build up a list of chain tips.  We start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    BOOST_FOREACH (const PAIRTYPE(const uint256, CBlockIndex*) & item, mapBlockIndex)
        setTips.insert(item.second);
    BOOST_FOREACH (const PAIRTYPE(const uint256, CBlockIndex*) & item, mapBlockIndex) {
        const CBlockIndex* pprev = item.second->pprev;
        if (pprev)
            setTips.erase(pprev);
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    UniValue res(UniValue::VARR);
    BOOST_FOREACH (const CBlockIndex* block, setTips) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("height", block->nHeight));
        obj.push_back(Pair("hash", block->phashBlock->GetHex()));

        const int branchLen = block->nHeight - chainActive.FindFork(block)->nHeight;
        obj.push_back(Pair("branchlen", branchLen));

        string status;
        if (chainActive.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nChainTx == 0) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.push_back(Pair("status", status));

        res.push_back(obj);
    }

    return res;
}

UniValue getfeeinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getfeeinfo blocks\n"
            "\nReturns details of transaction fees over the last n blocks.\n"

            "\nArguments:\n"
            "1. blocks     (int, required) the number of blocks to get transaction data from\n"

            "\nResult:\n"
            "{\n"
            "  \"txcount\": xxxxx                (numeric) Current tx count\n"
            "  \"txbytes\": xxxxx                (numeric) Sum of all tx sizes\n"
            "  \"ttlfee\": xxxxx                 (numeric) Sum of all fees\n"
            "  \"feeperkb\": xxxxx               (numeric) Average fee per kb over the block range\n"
            "  \"rec_highpriorityfee_perkb\": xxxxx    (numeric) Recommended fee per kb to use for a high priority tx\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getfeeinfo", "5") + HelpExampleRpc("getfeeinfo", "5"));

    LOCK(cs_main);

    int nBlocks = params[0].get_int();
    int nBestHeight = chainActive.Height();
    int nStartHeight = nBestHeight - nBlocks;
    if (nBlocks < 0 || nStartHeight <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid start height");

    CAmount nFees = 0;
    int64_t nBytes = 0;
    int64_t nTotal = 0;
    for (int i = nStartHeight; i <= nBestHeight; i++) {
        CBlockIndex* pindex = chainActive[i];
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex))
            throw JSONRPCError(RPC_DATABASE_ERROR, "failed to read block from disk");

        CAmount nValueIn = 0;
        CAmount nValueOut = 0;
        for (const CTransaction& tx : block.vtx) {
            if (tx.IsCoinBase() || tx.IsCoinStake())
                continue;

            for (unsigned int j = 0; j < tx.vin.size(); j++) {
                if (tx.vin[j].scriptSig.IsZerocoinSpend()) {
                    nValueIn += tx.vin[j].nSequence * COIN;
                    continue;
                }

                COutPoint prevout = tx.vin[j].prevout;
                CTransaction txPrev;
                uint256 hashBlock;
                if(!GetTransaction(prevout.hash, txPrev, hashBlock, true))
                    throw JSONRPCError(RPC_DATABASE_ERROR, "failed to read tx from disk");
                nValueIn += txPrev.vout[prevout.n].nValue;
            }

            for (unsigned int j = 0; j < tx.vout.size(); j++) {
                nValueOut += tx.vout[j].nValue;
            }

            nFees += nValueIn - nValueOut;
            nBytes += tx.GetSerializeSize(SER_NETWORK, CLIENT_VERSION);
            nTotal++;
        }

        pindex = chainActive.Next(pindex);
        if (!pindex)
            break;
    }

    UniValue ret(UniValue::VOBJ);
    CFeeRate nFeeRate = CFeeRate(nFees, nBytes);
    ret.push_back(Pair("txcount", (int64_t)nTotal));
    ret.push_back(Pair("txbytes", (int64_t)nBytes));
    ret.push_back(Pair("ttlfee", FormatMoney(nFees)));
    ret.push_back(Pair("feeperkb", FormatMoney(nFeeRate.GetFeePerK())));
    ret.push_back(Pair("rec_highpriorityfee_perkb", FormatMoney(nFeeRate.GetFeePerK() + 1000)));

    return ret;
}

UniValue mempoolInfoToJSON()
{
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("size", (int64_t) mempool.size()));
    ret.push_back(Pair("bytes", (int64_t) mempool.GetTotalTxSize()));
    //ret.push_back(Pair("usage", (int64_t) mempool.DynamicMemoryUsage()));

    return ret;
}

UniValue getmempoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"

            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx                (numeric) Current tx count\n"
            "  \"bytes\": xxxxx               (numeric) Sum of all tx sizes\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmempoolinfo", "") + HelpExampleRpc("getmempoolinfo", ""));

    return mempoolInfoToJSON();
}

UniValue invalidateblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock \"hash\"\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"

            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to mark as invalid\n"

            "\nExamples:\n" +
            HelpExampleCli("invalidateblock", "\"blockhash\"") + HelpExampleRpc("invalidateblock", "\"blockhash\""));

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        InvalidateBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue reconsiderblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock \"hash\"\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "This can be used to undo the effects of invalidateblock.\n"

            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to reconsider\n"

            "\nExamples:\n" +
            HelpExampleCli("reconsiderblock", "\"blockhash\"") + HelpExampleRpc("reconsiderblock", "\"blockhash\""));

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        ReconsiderBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue findserial(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1)
        throw runtime_error(
            "findserial \"serial\"\n"
            "\nSearches the zerocoin database for a zerocoin spend transaction that contains the specified serial\n"

            "\nArguments:\n"
            "1. serial   (string, required) the serial of a zerocoin spend to search for.\n"

            "\nResult:\n"
            "{\n"
            "  \"success\": true|false        (boolean) Whether the serial was found\n"
            "  \"txid\": \"xxx\"              (string) The transaction that contains the spent serial\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("findserial", "\"serial\"") + HelpExampleRpc("findserial", "\"serial\""));

    std::string strSerial = params[0].get_str();
    CBigNum bnSerial = 0;
    bnSerial.SetHex(strSerial);
    if (!bnSerial)
	throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid serial");

    uint256 txid = 0;
    bool fSuccess = zerocoinDB->ReadCoinSpend(bnSerial, txid);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("success", fSuccess));
    ret.push_back(Pair("txid", txid.GetHex()));
    return ret;
}

UniValue getaccumulatorvalues(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccumulatorvalues \"height\"\n"
                    "\nReturns the accumulator values associated with a block height\n"

                    "\nArguments:\n"
                    "1. height   (numeric, required) the height of the checkpoint.\n"

                    "\nExamples:\n" +
            HelpExampleCli("getaccumulatorvalues", "\"height\"") + HelpExampleRpc("getaccumulatorvalues", "\"height\""));

    int nHeight = params[0].get_int();

    CBlockIndex* pindex = chainActive[nHeight];
    if (!pindex)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid block height");

    UniValue ret(UniValue::VARR);
    for (libzerocoin::CoinDenomination denom : libzerocoin::zerocoinDenomList) {
        CBigNum bnValue;
        if(!GetAccumulatorValueFromDB(pindex->nAccumulatorCheckpoint, denom, bnValue))
            throw JSONRPCError(RPC_DATABASE_ERROR, "failed to find value in database");

        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair(std::to_string(denom), bnValue.GetHex()));
        ret.push_back(obj);
    }

    return ret;
}