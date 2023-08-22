// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activegamemaster.h"
#include "chainparams.h"
#include "llmq/quorums.h"
#include "llmq/quorums_blockprocessor.h"
#include "llmq/quorums_commitment.h"
#include "llmq/quorums_debug.h"
#include "llmq/quorums_dkgsession.h"
#include "llmq/quorums_signing.h"
#include "rpc/server.h"
#include "validation.h"

#include <string>

UniValue signsession(const JSONRPCRequest& request)
{
    if (!Params().IsTestChain()) {
        throw JSONRPCError(RPC_MISC_ERROR, "command available only for RegTest and TestNet network");
    }
    if (request.fHelp || (request.params.size() != 3)) {
        throw std::runtime_error(
            "signsession llmqType \"id\" \"msgHash\"\n"
            "\nArguments:\n"
            "1. llmqType              (int, required) LLMQ type.\n"
            "2. \"id\"                  (string, required) Request id.\n"
            "3. \"msgHash\"             (string, required) Message hash.\n"

            "\nResult:\n"
            "n      (bool) True if the sign was succesfull false otherwise\n"

            "\nExample:\n" +
            HelpExampleRpc("signsession", "100 \"xxx\", \"xxx\"") + HelpExampleCli("signsession", "100 \"xxx\", \"xxx\""));
    }
    Consensus::LLMQType llmqType = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmqType)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid LLMQ type");
    }

    uint256 id = ParseHashV(request.params[1], "id");
    uint256 msgHash = ParseHashV(request.params[2], "msgHash");
    return llmq::quorumSigningManager->AsyncSignIfMember(llmqType, id, msgHash);
}

UniValue hasrecoverysignature(const JSONRPCRequest& request)
{
    if (!Params().IsTestChain()) {
        throw JSONRPCError(RPC_MISC_ERROR, "command available only for RegTest and TestNet network");
    }
    if (request.fHelp || (request.params.size() != 3)) {
        throw std::runtime_error(
            "hasrecoverysignature llmqType \"id\" \"msgHash\"\n"
            "\nArguments:\n"
            "1. llmqType              (int, required) LLMQ type.\n"
            "2. \"id\"                  (string, required) Request id.\n"
            "3. \"msgHash\"             (string, required) Message hash.\n"

            "\nResult:\n"
            "n      (bool) True if you have already received a recovery signature for the given signing session\n"

            "\nExample:\n" +
            HelpExampleRpc("hasrecoverysignature", "100 \"xxx\", \"xxx\"") + HelpExampleCli("hasrecoverysignature", "100 \"xxx\", \"xxx\""));
    }
    Consensus::LLMQType llmqType = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmqType)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid LLMQ type");
    }

    uint256 id = ParseHashV(request.params[1], "id");
    uint256 msgHash = ParseHashV(request.params[2], "msgHash");
    return llmq::quorumSigningManager->HasRecoveredSig(llmqType, id, msgHash);
}

UniValue issessionconflicting(const JSONRPCRequest& request)
{
    if (!Params().IsTestChain()) {
        throw JSONRPCError(RPC_MISC_ERROR, "command available only for RegTest and TestNet network");
    }
    if (request.fHelp || (request.params.size() != 3)) {
        throw std::runtime_error(
            "issessionconflicting llmqType \"id\" \"msgHash\"\n"
            "\nArguments:\n"
            "1. llmqType              (int, required) LLMQ type.\n"
            "2. \"id\"                  (string, required) Request id.\n"
            "3. \"msgHash\"             (string, required) Message hash.\n"

            "\nResult:\n"
            "n      (bool) True if you have the recovery signature of an another signing session with same id but different msgHash\n"

            "\nExample:\n" +
            HelpExampleRpc("issessionconflicting", "100 \"xxx\", \"xxx\"") + HelpExampleCli("issessionconflicting", "100 \"xxx\", \"xxx\""));
    }
    Consensus::LLMQType llmqType = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmqType)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid LLMQ type");
    }

    uint256 id = ParseHashV(request.params[1], "id");
    uint256 msgHash = ParseHashV(request.params[2], "msgHash");
    return llmq::quorumSigningManager->IsConflicting(llmqType, id, msgHash);
}

UniValue listquorums(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            "listquorums ( count )\n"
            "\nArguments:\n"
            "1. count           (numeric, optional, default=10) Number of quorums to list\n"

            "\nResult:\n"
            "{\n"
            "  \"llmqType\": [      (array of string) A json array of quorum hashes for a given llmqType\n"
            "    \"quorumhash\",    (string) Block hash of the quorum\n"
            "    ...\n"
            "  ],\n"
            "  ...\n"
            "}\n"

            "\nExample:\n" +
            HelpExampleRpc("listquorums", "1") + HelpExampleCli("listquorums", "1"));
    }

    LOCK(cs_main);
    int count = 10;
    if(request.params.size() == 1) {
        count = request.params[0].get_int();
        if(count <= 0) {
            throw std::runtime_error(
            "count cannot be 0 or negative!\n");
        }
    }
    UniValue ret(UniValue::VOBJ);

    for (auto& p : Params().GetConsensus().llmqs) {
        UniValue v(UniValue::VARR);

        auto quorums = llmq::quorumManager->ScanQuorums(p.first, chainActive.Tip()->GetBlockHash(), count);
        for (auto& q : quorums) {
            v.push_back(q->pindexQuorum->GetBlockHash().ToString());
        }

        ret.pushKV(p.second.name, v);
    }


    return ret;
}

UniValue getquoruminfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 3 || request.params.size() < 2)
        throw std::runtime_error(
            "getquoruminfo llmqType \"quorumHash\" ( includeSkShare )\n"
            "\nArguments:\n"
            "1. llmqType              (numeric, required) LLMQ type.\n"
            "2. \"quorumHash\"          (string, required) Block hash of quorum.\n"
            "3. includeSkShare        (boolean, optional) Include secret key share in output.\n"

            "\nResult:\n"
            "{\n"
            "  \"height\": n,                     (numeric) The starting block height of the quorum\n"
            "  \"quorumHash\": \"quorumHash\",      (string) Block hash of the quorum\n"
            "  \"members\": [                     (array of json objects)\n"
            "     {\n"
            "       \"proTxHash\": \"proTxHash\"    (string) ProTxHash of the quorum member\n"
            "       \"valid\": true/false         (boolean) True/false if the member is valid/invalid\n"
            "       \"pubKeyShare\": pubKeyShare  (string) Quorum public key share of the member, will be outputed only if the command is performed by another quorum member or watcher\n"
            "     },\n"
            "     ...\n"
            "   ],\n"
            "   \"quorumPublicKey\": quorumPublicKey,   (string) Public key of the quorum\n"
            "   \"secretKeyShare\": secretKeyShare      (string) This is outputed only if includeSkShare=true and the command is performed by a valid member of the quorum. It corresponds to the secret key share of that member\n"
            "}\n"

            "\nExample:\n" +
            HelpExampleRpc("getquoruminfo", "2 \"xxx\", true") + HelpExampleCli("getquoruminfo", "2, \"xxx\",true"));

    LOCK(cs_main);

    Consensus::LLMQType llmqType = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmqType)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid llmqType");
    }

    uint256 blockHash = ParseHashV(request.params[1], "quorumHash");
    bool includeSkShare = false;
    if (request.params.size() > 2) {
        includeSkShare = request.params[2].get_bool();
    }

    auto quorum = llmq::quorumManager->GetQuorum(llmqType, blockHash);
    if (!quorum) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "quorum not found");
    }

    UniValue ret(UniValue::VOBJ);

    ret.pushKV("height", quorum->pindexQuorum->nHeight);
    ret.pushKV("quorumHash", quorum->pindexQuorum->GetBlockHash().ToString());

    UniValue membersArr(UniValue::VARR);
    for (size_t i = 0; i < quorum->members.size(); i++) {
        auto& dgm = quorum->members[i];
        UniValue mo(UniValue::VOBJ);
        mo.pushKV("proTxHash", dgm->proTxHash.ToString());
        mo.pushKV("valid", quorum->validMembers[i]);
        if (quorum->validMembers[i]) {
            CBLSPublicKey pubKey = quorum->GetPubKeyShare(i);
            if (pubKey.IsValid()) {
                mo.pushKV("pubKeyShare", pubKey.ToString());
            }
        }
        membersArr.push_back(mo);
    }

    ret.pushKV("members", membersArr);
    ret.pushKV("quorumPublicKey", quorum->quorumPublicKey.ToString());
    CBLSSecretKey skShare = quorum->GetSkShare();
    if (includeSkShare && skShare.IsValid()) {
        ret.pushKV("secretKeyShare", skShare.ToString());
    }

    return ret;
}

UniValue getminedcommitment(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
                "getminedcommitment llmq_type quorum_hash\n"
                "Return information about the commitment for given quorum.\n"
                "\nArguments:\n"
                "1. llmq_type         (number, required) LLMQ type.\n"
                "2. quorum_hash       (hex string, required) LLMQ hash.\n"
                "\nExamples:\n"
                + HelpExampleRpc("getminedcommitment", "2 \"xxx\"")
                + HelpExampleCli("getminedcommitment", "2, \"xxx\"")
        );
    }

    Consensus::LLMQType llmq_type = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmq_type)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid llmq_type");
    }
    const uint256& quorum_hash = ParseHashV(request.params[1], "quorum_hash");
    if (WITH_LOCK(cs_main, return LookupBlockIndex(quorum_hash)) == nullptr) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid quorum_hash");
    }

    llmq::CFinalCommitment qc;
    uint256 block_hash;
    if (!llmq::quorumBlockProcessor->GetMinedCommitment(llmq_type, quorum_hash, qc, block_hash)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "mined commitment not found");
    }

    UniValue ret(UniValue::VOBJ);
    qc.ToJson(ret);
    ret.pushKV("block_hash", block_hash.ToString());
    return ret;
}

UniValue getquorummembers(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
                "getquorummembers llmq_type quorum_hash\n"
                "Return the list of proTx hashes for given quorum.\n"
                "\nArguments:\n"
                "1. llmq_type         (number, required) LLMQ type.\n"
                "2. quorum_hash       (hex string, required) LLMQ hash.\n"
                "\nExamples:\n"
                + HelpExampleRpc("getquorummembers", "2 \"xxx\"")
                + HelpExampleCli("getquorummembers", "2, \"xxx\"")
        );
    }

    Consensus::LLMQType llmq_type = static_cast<Consensus::LLMQType>(request.params[0].get_int());
    if (!Params().GetConsensus().llmqs.count(llmq_type)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid llmq_type");
    }

    const uint256& quorum_hash = ParseHashV(request.params[1], "quorum_hash");
    const CBlockIndex* pindexQuorum = WITH_LOCK(cs_main, return LookupBlockIndex(quorum_hash));
    if (pindexQuorum == nullptr) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid quorum_hash");
    }

    auto gms = deterministicGMManager->GetAllQuorumMembers(llmq_type, pindexQuorum);
    UniValue ret(UniValue::VARR);
    for (const auto& dgm : gms) {
        ret.push_back(dgm->proTxHash.ToString());
    }
    return ret;
}

UniValue quorumdkgstatus(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
                "quorumdkgstatus ( detail_level )\n"
                "Return the status of the current DKG process of the active gamemaster.\n"
                "\nArguments:\n"
                "1. detail_level         (number, optional, default=0) Detail level of output.\n"
                "                        0=Only show counts. 1=Show member indexes. 2=Show member's ProTxHashes.\n"
                "\nExamples:\n"
                + HelpExampleRpc("quorumdkgstatus", "2")
                + HelpExampleCli("quorumdkgstatus", "")
        );
    }

    int detailLevel = request.params.size() > 0 ? request.params[0].get_int() : 0;
    if (detailLevel < 0 || detailLevel > 2) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("invalid detail_level %d", detailLevel));
    }

    if (!fGameMaster || !activeGamemasterManager) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "This is not a (deterministic) gamemaster");
    }

    llmq::CDKGDebugStatus status;
    llmq::quorumDKGDebugManager->GetLocalDebugStatus(status);

    auto ret = status.ToJson(detailLevel);

    const int tipHeight = WITH_LOCK(cs_main, return chainActive.Height(); );

    UniValue minableCommitments(UniValue::VOBJ);
    for (const auto& p : Params().GetConsensus().llmqs) {
        auto& params = p.second;
        llmq::CFinalCommitment fqc;
        if (llmq::quorumBlockProcessor->GetMinableCommitment(params.type, tipHeight, fqc)) {
            UniValue obj(UniValue::VOBJ);
            fqc.ToJson(obj);
            minableCommitments.pushKV(params.name, obj);
        }
    }
    ret.pushKV("minableCommitments", minableCommitments);

    return ret;
}

UniValue quorumdkgsimerror(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
                "quorumdkgsimerror \"error_type\" rate\n"
                "This enables simulation of errors and malicious behaviour in the DKG.\n"
                "Only available on testnet/regtest for LLMQ_TEST llmq type.\n"
                "\nArguments:\n"
                "1. \"error_type\"          (string, required) Error type.\n"
                "2. rate                  (number, required) Rate at which to simulate this error type.\n"
                "\nExamples:\n"
                + HelpExampleRpc("quorumdkgsimerror", "\"justify-lie\", 0.1")
                + HelpExampleCli("quorumdkgsimerror", "\"justify-lie\" 0.1")
        );
    }

    if (!Params().IsTestChain()) {
        throw JSONRPCError(RPC_MISC_ERROR, "This command cannot be used on main net.");
    }

    std::string error_type = request.params[0].get_str();
    double rate = ParseDoubleV(request.params[1], "rate");
    if (rate < 0 || rate > 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "invalid rate. Must be between 0 and 1");
    }

    if (!llmq::SetSimulatedDKGErrorRate(error_type, rate)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("invalid error_type: %s", error_type));
    }

    return NullUniValue;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category       name                      actor (function)      okSafe argNames
  //  -------------- ------------------------- --------------------- ------ --------
    { "evo",         "getminedcommitment",     &getminedcommitment,  true,  {"llmq_type", "quorum_hash"}  },
    { "evo",         "getquorummembers",       &getquorummembers,    true,  {"llmq_type", "quorum_hash"}  },
    { "evo",         "quorumdkgsimerror",      &quorumdkgsimerror,   true,  {"error_type", "rate"}  },
    { "evo",         "quorumdkgstatus",        &quorumdkgstatus,     true,  {"detail_level"}  },
    { "evo",         "listquorums",            &listquorums,         true,  {"count"}  },
    { "evo",         "getquoruminfo",          &getquoruminfo,       true,  {"llmqType", "quorumHash", "includeSkShare"}  },

    /** Not shown in help */
    { "hidden",      "signsession",            &signsession,         true,  {"llmqType", "id", "msgHash"} },
    { "hidden",      "hasrecoverysignature",   &hasrecoverysignature,true,  {"llmqType", "id", "msgHash"} },
    { "hidden",      "issessionconflicting",   &issessionconflicting,true,  {"llmqType", "id", "msgHash"} },
 };
// clang-format on

void RegisterQuorumsRPCCommands(CRPCTable& tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
