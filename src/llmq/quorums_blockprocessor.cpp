// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_blockprocessor.h"

#include "bls/key_io.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "evo/evodb.h"
#include "evo/specialtx_validation.h"
#include "llmq/quorums_utils.h"
#include "net.h"
#include "primitives/block.h"
#include "quorums_debug.h"
#include "spork.h"
#include "validation.h"

namespace llmq
{
std::unique_ptr<CQuorumBlockProcessor> quorumBlockProcessor{nullptr};

static const std::string DB_MINED_COMMITMENT = "q_mc";
static const std::string DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT = "q_mcih";

CQuorumBlockProcessor::CQuorumBlockProcessor(CEvoDB &_evoDb) :
    evoDb(_evoDb)
{
    // TODO: add unordered lru cache
    //utils::InitQuorumsCache(mapHasMinedCommitmentCache);
}

template<typename... Args>
static int LogMisbehaving(CNode* pfrom, int nDoS, const char* fmt, const Args&... args)
{
    try {
        LogPrint(BCLog::LLMQ, "Invalid QFCOMMITMENT message from peer=%d (reason: %s)\n",
                pfrom->GetId(), tfm::format(fmt, args...));
    } catch (tinyformat::format_error &e) {
        LogPrintf("Error (%s) while formatting message %s\n", std::string(e.what()), fmt);
    }
    return nDoS;
}

void CQuorumBlockProcessor::ProcessMessage(CNode* pfrom, CDataStream& vRecv, int& retMisbehavingScore)
{
    AssertLockNotHeld(cs_main);
    CFinalCommitment qc;
    vRecv >> qc;

    uint256 qfc_hash{::SerializeHash(qc)};
    {
        LOCK(cs_main);
        g_connman->RemoveAskFor(qfc_hash, MSG_QUORUM_FINAL_COMMITMENT);
    }

    if (qc.IsNull()) {
        retMisbehavingScore = LogMisbehaving(pfrom, 100, "null commitment");
        return;
    }

    // Check if we already got a better one locally
    // We do this before verifying the commitment to avoid DoS
    if (HasBetterMinableCommitment(qc)) {
        return;
    }

    CValidationState state;
    if (!WITH_LOCK(cs_main, return VerifyLLMQCommitment(qc, chainActive.Tip(), state); )) {
        // Not punishable reject reasons
        static std::set<std::string> not_punishable_reasons = {
                // can't really punish the node for "bad-qc-quorum-hash" here, as we might simply be
                // the one that is on the wrong chain or not fully synced
                "bad-qc-quorum-hash-not-found", "bad-qc-quorum-hash-not-active-chain"
        };

        int dos = (not_punishable_reasons.count(state.GetRejectReason()) ? 0 : state.GetDoSScore());
        retMisbehavingScore = LogMisbehaving(pfrom, dos, "invalid commtiment for quorum %s: %s",
                                             qc.quorumHash.ToString(), state.GetRejectReason());
        return;
    }

    LogPrintf("%s :received commitment for quorum %s:%d, validMembers=%d, signers=%d, peer=%d\n", __func__,
              qc.quorumHash.ToString(), qc.llmqType, qc.CountValidMembers(), qc.CountSigners(), pfrom->GetId());

    AddAndRelayMinableCommitment(qc, &qfc_hash);
}

bool CQuorumBlockProcessor::ProcessBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& state, bool fJustCheck)
{
    LOCK(cs_main);
    const auto& consensus = Params().GetConsensus();

    bool fDIP3Active = consensus.NetworkUpgradeActive(pindex->nHeight, Consensus::UPGRADE_V6_0);
    if (!fDIP3Active) {
        return true;
    }

    std::map<Consensus::LLMQType, CFinalCommitment> qcs;
    if (!GetCommitmentsFromBlock(block, pindex, qcs, state)) {
        return false;
    }

    // The following checks make sure that there is always a (possibly null) commitment while in the mining phase
    // until the first non-null commitment has been mined. After the non-null commitment, no other commitments are
    // allowed, including null commitments.
    // Note: must only check quorums that were enabled at the _previous_ block height to match mining logic
    for (const auto& llmq : consensus.llmqs) {
        // skip these checks when replaying blocks after the crash
        if (WITH_LOCK(cs_main, return chainActive.Tip(); ) == nullptr) {
            break;
        }

        // does the currently processed block contain a (possibly null) commitment for the current session?
        Consensus::LLMQType type = llmq.first;
        bool hasCommitmentInNewBlock = qcs.count(type) != 0;
        bool isCommitmentRequired = IsCommitmentRequired(type, pindex->nHeight);

        if (hasCommitmentInNewBlock && !isCommitmentRequired) {
            // If we're either not in the mining phase or a non-null commitment was mined already, reject the block
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-not-allowed");
        }

        if (!hasCommitmentInNewBlock && isCommitmentRequired) {
            // If no non-null commitment was mined for the mining phase yet and the new block does not include
            // a (possibly null) commitment, the block should be rejected.
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-missing");
        }
    }

    const uint256& blockHash = block.GetHash();

    for (const auto& p : qcs) {
        const auto& qc = p.second;
        if (!ProcessCommitment(pindex->nHeight, blockHash, qc, state, fJustCheck)) {
            return false;
        }
    }

    return true;
}

// We store a mapping from minedHeight->quorumHeight in the DB
// minedHeight is inversed so that entries are traversable in reversed order
static std::tuple<std::string, uint8_t, uint32_t> BuildInversedHeightKey(Consensus::LLMQType llmqType, int nMinedHeight)
{
    // nMinedHeight must be converted to big endian to make it comparable when serialized
    return std::make_tuple(DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT, static_cast<uint8_t>(llmqType), htobe32(std::numeric_limits<uint32_t>::max() - nMinedHeight));
}

bool CQuorumBlockProcessor::ProcessCommitment(int nHeight, const uint256& blockHash, const CFinalCommitment& qc, CValidationState& state, bool fJustCheck)
{
    // skip `bad-qc-block` checks below when replaying blocks after a crash
    const uint256& quorumHash = WITH_LOCK(cs_main, return chainActive.Tip(); ) != nullptr
                              ? GetQuorumBlockHash((Consensus::LLMQType)qc.llmqType, nHeight)
                              : qc.quorumHash;

    if (quorumHash.IsNull()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-null-quorumhash");
    }
    if (quorumHash != qc.quorumHash) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-block");
    }

    // index of quorumHash (and commitment signature) already checked
    const CBlockIndex* quorumIndex = mapBlockIndex.at(quorumHash);

    if (fJustCheck || qc.IsNull()) {
        return true;
    }

    // Store commitment in DB
    auto cacheKey = std::make_pair(qc.llmqType, quorumHash);
    evoDb.Write(std::make_pair(DB_MINED_COMMITMENT, cacheKey), std::make_pair(qc, blockHash));
    evoDb.Write(BuildInversedHeightKey((Consensus::LLMQType)qc.llmqType, nHeight), quorumIndex->nHeight);

    {
        LOCK(minableCommitmentsCs);
        //mapHasMinedCommitmentCache[qc.llmqType].erase(qc.quorumHash);
        minableCommitmentsByQuorum.erase(cacheKey);
        minableCommitments.erase(::SerializeHash(qc));
    }

    LogPrintf("%s: processed commitment from block. type=%d, quorumHash=%s, signers=%s, validMembers=%d, quorumPublicKey=%s\n", __func__,
              qc.llmqType, quorumHash.ToString(), qc.CountSigners(), qc.CountValidMembers(), bls::EncodePublic(Params(), qc.quorumPublicKey));

    return true;
}

bool CQuorumBlockProcessor::UndoBlock(const CBlock& block, const CBlockIndex* pindex)
{
    std::map<Consensus::LLMQType, CFinalCommitment> qcs;
    CValidationState dummy;
    if (!GetCommitmentsFromBlock(block, pindex, qcs, dummy)) {
        return false;
    }

    for (const auto& p : qcs) {
        const auto& qc = p.second;
        if (qc.IsNull()) {
            continue;
        }

        evoDb.Erase(std::make_pair(DB_MINED_COMMITMENT, std::make_pair(static_cast<uint8_t>(qc.llmqType), qc.quorumHash)));
        evoDb.Erase(BuildInversedHeightKey((Consensus::LLMQType)qc.llmqType, pindex->nHeight));
        {
            LOCK(minableCommitmentsCs);
            //mapHasMinedCommitmentCache[qc.llmqType].erase(qc.quorumHash);
        }

        // if a reorg happened, we should allow to mine this commitment later
        if (!HasBetterMinableCommitment(qc)) {
            AddAndRelayMinableCommitment(qc);
        }
    }

    return true;
}

bool CQuorumBlockProcessor::GetCommitmentsFromBlock(const CBlock& block, const CBlockIndex* pindex, std::map<Consensus::LLMQType, CFinalCommitment>& ret, CValidationState& state)
{
    ret.clear();

    for (const auto& tx : block.vtx) {
        if (!tx->IsQuorumCommitmentTx()) {
            continue;
        }
        LLMQCommPL pl;
        if (!GetTxPayload(*tx, pl)) {
            // should not happen as it was verified before processing the block
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-payload");
        }

        // only allow one commitment per type and per block
        if (ret.count((Consensus::LLMQType)pl.commitment.llmqType)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-dup");
        }

        ret.emplace((Consensus::LLMQType)pl.commitment.llmqType, std::move(pl.commitment));
    }

    return true;
}

bool CQuorumBlockProcessor::IsMiningPhase(Consensus::LLMQType llmqType, int nHeight)
{
    const auto& params = Params().GetConsensus().llmqs.at(llmqType);
    int phaseIndex = nHeight % params.dkgInterval;
    return phaseIndex >= params.dkgMiningWindowStart && phaseIndex <= params.dkgMiningWindowEnd;
}

bool CQuorumBlockProcessor::IsCommitmentRequired(Consensus::LLMQType llmqType, int nHeight)
{
    const uint256& quorumHash = GetQuorumBlockHash(llmqType, nHeight);

    // perform extra check for quorumHash.IsNull as the quorum hash is unknown for the first block of a session
    // this is because the currently processed block's hash will be the quorumHash of this session
    bool isMiningPhase = !quorumHash.IsNull() && IsMiningPhase(llmqType, nHeight);

    // did we already mine a non-null commitment for this session?
    bool hasMinedCommitment = !quorumHash.IsNull() && HasMinedCommitment(llmqType, quorumHash);

    return isMiningPhase && !hasMinedCommitment;
}

// This method returns UINT256_ZERO on the first block of the DKG interval (because the block hash is not known yet)
uint256 CQuorumBlockProcessor::GetQuorumBlockHash(Consensus::LLMQType llmqType, int nHeight)
{
    auto& params = Params().GetConsensus().llmqs.at(llmqType);
    int quorumStartHeight = nHeight - (nHeight % params.dkgInterval);

    LOCK(cs_main);
    if (quorumStartHeight > chainActive.Height()) {
        return UINT256_ZERO;
    }
    return chainActive[quorumStartHeight]->GetBlockHash();
}

bool CQuorumBlockProcessor::HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash)
{
    bool fExists;
    /*
    {
        LOCK(minableCommitmentsCs);
        if (mapHasMinedCommitmentCache[llmqType].get(quorumHash, fExists)) {
            return fExists;
        }
    }
    */

    fExists = evoDb.Exists(std::make_pair(DB_MINED_COMMITMENT, std::make_pair(static_cast<uint8_t>(llmqType), quorumHash)));

    /*
    LOCK(minableCommitmentsCs);
    mapHasMinedCommitmentCache[llmqType].insert(quorumHash, fExists);
    */

    return fExists;
}

bool CQuorumBlockProcessor::GetMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash, CFinalCommitment& retQc, uint256& retMinedBlockHash)
{
    auto key = std::make_pair(DB_MINED_COMMITMENT, std::make_pair(static_cast<uint8_t>(llmqType), quorumHash));
    std::pair<CFinalCommitment, uint256> p;
    if (!evoDb.Read(key, p)) {
        return false;
    }
    retQc = std::move(p.first);
    retMinedBlockHash = p.second;
    return true;
}

// The returned quorums are in reversed order, so the most recent one is at index 0
std::vector<const CBlockIndex*> CQuorumBlockProcessor::GetMinedCommitmentsUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, size_t maxCount)
{
    LOCK(evoDb.cs);

    auto dbIt = evoDb.GetCurTransaction().NewIteratorUniquePtr();

    auto firstKey = BuildInversedHeightKey(llmqType, pindex->nHeight);
    auto lastKey = BuildInversedHeightKey(llmqType, 0);

    dbIt->Seek(firstKey);

    std::vector<const CBlockIndex*> ret;
    ret.reserve(maxCount);

    while (dbIt->Valid() && ret.size() < maxCount) {
        decltype(firstKey) curKey;
        int quorumHeight;
        if (!dbIt->GetKey(curKey) || curKey >= lastKey) {
            break;
        }
        if (std::get<0>(curKey) != DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT || std::get<1>(curKey) != static_cast<uint8_t>(llmqType)) {
            break;
        }

        uint32_t nMinedHeight = std::numeric_limits<uint32_t>::max() - be32toh(std::get<2>(curKey));
        if (nMinedHeight > (uint32_t) pindex->nHeight) {
            break;
        }

        if (!dbIt->GetValue(quorumHeight)) {
            break;
        }

        auto quorumIndex = pindex->GetAncestor(quorumHeight);
        assert(quorumIndex);
        ret.emplace_back(quorumIndex);

        dbIt->Next();
    }

    return ret;
}

// The returned quorums are in reversed order, so the most recent one is at index 0
std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> CQuorumBlockProcessor::GetMinedAndActiveCommitmentsUntilBlock(const CBlockIndex* pindex)
{
    std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> ret;

    for (const auto& p : Params().GetConsensus().llmqs) {
        auto& v = ret[p.second.type];
        v.reserve(p.second.signingActiveQuorumCount);
        auto commitments = GetMinedCommitmentsUntilBlock(p.second.type, pindex, p.second.signingActiveQuorumCount);
        for (auto& c : commitments) {
            v.emplace_back(c);
        }
    }

    return ret;
}

bool CQuorumBlockProcessor::HasMinableCommitment(const uint256& hash)
{
    LOCK(minableCommitmentsCs);
    return minableCommitments.count(hash) != 0;
}

bool CQuorumBlockProcessor::HasBetterMinableCommitment(const CFinalCommitment& qc)
{
    LOCK(minableCommitmentsCs);
    auto it = minableCommitmentsByQuorum.find(std::make_pair(qc.llmqType, qc.quorumHash));
    if (it != minableCommitmentsByQuorum.end()) {
        auto jt = minableCommitments.find(it->second);
        return jt != minableCommitments.end() && jt->second.CountSigners() >= qc.CountSigners();
    }
    return false;
}

void CQuorumBlockProcessor::AddAndRelayMinableCommitment(const CFinalCommitment& fqc, uint256* cached_fqc_hash)
{
    const uint256& commitmentHash = cached_fqc_hash ? *cached_fqc_hash : ::SerializeHash(fqc);
    {
        LOCK(minableCommitmentsCs);
        auto k = std::make_pair(fqc.llmqType, fqc.quorumHash);
        auto ins = minableCommitmentsByQuorum.emplace(k, commitmentHash);
        if (!ins.second) {
            // remove old commitment
            minableCommitments.erase(ins.first->second);
            // update commitment hash to the new one
            ins.first->second = commitmentHash;
        }
        // add new commitment
        minableCommitments.emplace(commitmentHash, fqc);
        quorumDKGDebugManager->UpdateLocalSessionStatus((Consensus::LLMQType)fqc.llmqType, [&](CDKGDebugSessionStatus& status) {
            if (status.quorumHash != fqc.quorumHash || status.receivedFinalCommitment) {
                return false;
            }
            status.receivedFinalCommitment = true;
            return true;
        });
    }
    // relay commitment inv (if DKG is not in maintenance)
    if (!sporkManager.IsSporkActive(SPORK_22_LLMQ_DKG_MAINTENANCE)) {
        CInv inv(MSG_QUORUM_FINAL_COMMITMENT, commitmentHash);
        g_connman->RelayInv(inv);
    }
}

bool CQuorumBlockProcessor::GetMinableCommitmentByHash(const uint256& commitmentHash, llmq::CFinalCommitment& ret)
{
    LOCK(minableCommitmentsCs);
    auto it = minableCommitments.find(commitmentHash);
    if (it == minableCommitments.end()) {
        return false;
    }
    ret = it->second;
    return true;
}

// Will return false if no commitment should be mined
// Will return true and a null commitment if no minable commitment is known and none was mined yet
bool CQuorumBlockProcessor::GetMinableCommitment(Consensus::LLMQType llmqType, int nHeight, CFinalCommitment& ret)
{
    if (!IsCommitmentRequired(llmqType, nHeight)) {
        // no commitment required
        return false;
    }

    uint256 quorumHash = GetQuorumBlockHash(llmqType, nHeight);
    if (quorumHash.IsNull()) {
        return false;
    }

    if (sporkManager.IsSporkActive(SPORK_22_LLMQ_DKG_MAINTENANCE)) {
        // null commitment required
        ret = CFinalCommitment(Params().GetConsensus().llmqs.at(llmqType), quorumHash);
        return true;
    }

    LOCK(minableCommitmentsCs);

    auto k = std::make_pair(static_cast<uint8_t>(llmqType), quorumHash);
    auto it = minableCommitmentsByQuorum.find(k);
    if (it == minableCommitmentsByQuorum.end()) {
        // null commitment required
        ret = CFinalCommitment(Params().GetConsensus().llmqs.at(llmqType), quorumHash);
        return true;
    }

    ret = minableCommitments.at(it->second);

    return true;
}

bool CQuorumBlockProcessor::GetMinableCommitmentTx(Consensus::LLMQType llmqType, int nHeight, CTransactionRef& ret)
{
    LLMQCommPL pl;
    if (!GetMinableCommitment(llmqType, nHeight, pl.commitment)) {
        return false;
    }

    pl.nHeight = nHeight;

    CMutableTransaction tx;
    tx.nVersion = CTransaction::TxVersion::SAPLING;
    tx.nType = CTransaction::TxType::LLMQCOMM;
    SetTxPayload(tx, pl);

    ret = MakeTransactionRef(tx);

    return true;
}

} // namespace llmq
