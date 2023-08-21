// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_debug.h"

#include "chainparams.h"
#include "evo/deterministicgms.h"
#include "llmq/quorums_utils.h"
#include "utiltime.h"
#include "validation.h"

namespace llmq
{

std::unique_ptr<CDKGDebugManager> quorumDKGDebugManager{nullptr};

UniValue CDKGDebugSessionStatus::ToJson(int detailLevel) const
{
    UniValue ret(UniValue::VOBJ);

    if (!Params().GetConsensus().llmqs.count((Consensus::LLMQType)llmqType) || quorumHash.IsNull()) {
        return ret;
    }

    std::vector<CDeterministicGMCPtr> dgmMembers;
    if (detailLevel == 2) {
        const CBlockIndex* pindexQuorum = WITH_LOCK(cs_main, return LookupBlockIndex(quorumHash));
        if (pindexQuorum != nullptr) {
            dgmMembers = deterministicGMManager->GetAllQuorumMembers((Consensus::LLMQType) llmqType, pindexQuorum);
        }
    }

    ret.pushKV("llmqType", llmqType);
    ret.pushKV("quorumHash", quorumHash.ToString());
    ret.pushKV("quorumHeight", (int)quorumHeight);
    ret.pushKV("phase", (int)phase);

    ret.pushKV("sentContributions", sentContributions);
    ret.pushKV("sentComplaint", sentComplaint);
    ret.pushKV("sentJustification", sentJustification);
    ret.pushKV("sentPrematureCommitment", sentPrematureCommitment);
    ret.pushKV("aborted", aborted);

    struct ArrOrCount {
        int count{0};
        UniValue arr{UniValue::VARR};
    };

    ArrOrCount badMembers;
    ArrOrCount weComplain;
    ArrOrCount receivedContributions;
    ArrOrCount receivedComplaints;
    ArrOrCount receivedJustifications;
    ArrOrCount receivedPrematureCommitments;
    ArrOrCount complaintsFromMembers;

    auto add = [&](ArrOrCount& v, size_t idx, bool flag) {
        if (flag) {
            if (detailLevel == 0) {
                v.count++;
            } else if (detailLevel == 1) {
                v.arr.push_back((int)idx);
            } else if (detailLevel == 2) {
                UniValue a(UniValue::VOBJ);
                a.pushKV("memberIndex", (int)idx);
                if (idx < dgmMembers.size()) {
                    a.pushKV("proTxHash", dgmMembers[idx]->proTxHash.ToString());
                }
                v.arr.push_back(a);
            }
        }
    };
    auto push = [&](ArrOrCount& v, const std::string& name) {
        if (detailLevel == 0) {
            ret.pushKV(name, v.count);
        } else {
            ret.pushKV(name, v.arr);
        }
    };

    for (size_t i = 0; i < members.size(); i++) {
        const auto& m = members[i];
        add(badMembers, i, m.bad);
        add(weComplain, i, m.weComplain);
        add(receivedContributions, i, m.receivedContribution);
        add(receivedComplaints, i, m.receivedComplaint);
        add(receivedJustifications, i, m.receivedJustification);
        add(receivedPrematureCommitments, i, m.receivedPrematureCommitment);
    }
    push(badMembers, "badMembers");
    push(weComplain, "weComplain");
    push(receivedContributions, "receivedContributions");
    push(receivedComplaints, "receivedComplaints");
    push(receivedJustifications, "receivedJustifications");
    push(receivedPrematureCommitments, "receivedPrematureCommitments");
    ret.pushKV("receivedFinalCommitment", receivedFinalCommitment);

    if (detailLevel == 2) {
        UniValue arr(UniValue::VARR);
        for (const auto& dgm : dgmMembers) {
            arr.push_back(dgm->proTxHash.ToString());
        }
        ret.pushKV("allMembers", arr);
    }

    return ret;
}

UniValue CDKGDebugStatus::ToJson(int detailLevel) const
{
    const Consensus::Params& consensus = Params().GetConsensus();
    UniValue ret(UniValue::VOBJ);

    ret.pushKV("time", nTime);
    ret.pushKV("timeStr", FormatISO8601DateTime(nTime));
    UniValue sessionsJson(UniValue::VOBJ);
    for (const auto& p : sessions) {
        Optional<Consensus::LLMQParams> opt_params = consensus.GetLLMQParams((Consensus::LLMQType)p.first);
        if (opt_params == nullopt) {
            continue;
        }
        sessionsJson.pushKV(opt_params->name, p.second.ToJson(detailLevel));
    }
    ret.pushKV("session", sessionsJson);

    return ret;
}

void CDKGDebugManager::GetLocalDebugStatus(llmq::CDKGDebugStatus& ret)
{
    LOCK(cs);
    ret = localStatus;
}

void CDKGDebugManager::ResetLocalSessionStatus(Consensus::LLMQType llmqType)
{
    LOCK(cs);

    auto it = localStatus.sessions.find(llmqType);
    if (it == localStatus.sessions.end()) {
        return;
    }

    localStatus.sessions.erase(it);
    localStatus.nTime = GetAdjustedTime();
}

void CDKGDebugManager::InitLocalSessionStatus(Consensus::LLMQType llmqType, const uint256& quorumHash, int quorumHeight)
{
    LOCK(cs);

    auto it = localStatus.sessions.find(llmqType);
    if (it == localStatus.sessions.end()) {
        it = localStatus.sessions.emplace(llmqType, CDKGDebugSessionStatus()).first;
    }

    auto& params = Params().GetConsensus().llmqs.at(llmqType);
    auto& session = it->second;
    session.llmqType = llmqType;
    session.quorumHash = quorumHash;
    session.quorumHeight = (uint32_t)quorumHeight;
    session.phase = 0;
    session.statusBitset = 0;
    session.members.clear();
    session.members.resize((size_t)params.size);
}

void CDKGDebugManager::UpdateLocalSessionStatus(Consensus::LLMQType llmqType, std::function<bool(CDKGDebugSessionStatus& status)>&& func)
{
    LOCK(cs);

    auto it = localStatus.sessions.find(llmqType);
    if (it == localStatus.sessions.end()) {
        return;
    }

    if (func(it->second)) {
        localStatus.nTime = GetAdjustedTime();
    }
}

void CDKGDebugManager::UpdateLocalMemberStatus(Consensus::LLMQType llmqType, size_t memberIdx, std::function<bool(CDKGDebugMemberStatus& status)>&& func)
{
    LOCK(cs);

    auto it = localStatus.sessions.find(llmqType);
    if (it == localStatus.sessions.end()) {
        return;
    }

    if (func(it->second.members.at(memberIdx))) {
        localStatus.nTime = GetAdjustedTime();
    }
}

} // namespace llmq
