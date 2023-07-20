// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_LLMQ_BLOCKPROCESSOR_H
#define hemis_LLMQ_BLOCKPROCESSOR_H

#include "consensus/params.h"
#include "llmq/quorums_commitment.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "uint256.h"

#include <map>

class CBlock;
class CBlockIndex;
class CConnman;
class CDataStream;
class CEvoDB;
class CNode;
class CValidationState;

namespace llmq
{

class CQuorumBlockProcessor
{
private:
    CEvoDB& evoDb;

    // TODO cleanup
    mutable RecursiveMutex minableCommitmentsCs;
    // <llmqType, quorumHash> --> commitment hash
    std::map<std::pair<uint8_t, uint256>, uint256> minableCommitmentsByQuorum;
    // commitment hash --> final commitment
    std::map<uint256, CFinalCommitment> minableCommitments;

public:
    explicit CQuorumBlockProcessor(CEvoDB& _evoDb);

    void ProcessMessage(CNode* pfrom, CDataStream& vRecv, int& retMisbehavingScore);

    bool ProcessBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& state, bool fJustCheck);
    bool UndoBlock(const CBlock& block, const CBlockIndex* pindex);

    void AddAndRelayMinableCommitment(const CFinalCommitment& fqc, uint256* cached_fqc_hash = nullptr);
    bool HasMinableCommitment(const uint256& hash);
    bool HasBetterMinableCommitment(const CFinalCommitment& qc);
    bool GetMinableCommitmentByHash(const uint256& commitmentHash, CFinalCommitment& ret);
    bool GetMinableCommitment(Consensus::LLMQType llmqType, int nHeight, CFinalCommitment& ret);
    bool GetMinableCommitmentTx(Consensus::LLMQType llmqType, int nHeight, CTransactionRef& ret);

    bool HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash);
    bool GetMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash, CFinalCommitment& ret, uint256& retMinedBlockHash);

    std::vector<const CBlockIndex*> GetMinedCommitmentsUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, size_t maxCount);
    std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> GetMinedAndActiveCommitmentsUntilBlock(const CBlockIndex* pindex);

private:
    static bool GetCommitmentsFromBlock(const CBlock& block, const CBlockIndex* pindex, std::map<Consensus::LLMQType, CFinalCommitment>& ret, CValidationState& state);
    bool ProcessCommitment(int nHeight, const uint256& blockHash, const CFinalCommitment& qc, CValidationState& state, bool fJustCheck);
    static bool IsMiningPhase(Consensus::LLMQType llmqType, int nHeight);
    bool IsCommitmentRequired(Consensus::LLMQType llmqType, int nHeight);
    static uint256 GetQuorumBlockHash(Consensus::LLMQType llmqType, int nHeight);
};

extern std::unique_ptr<CQuorumBlockProcessor> quorumBlockProcessor;

} // namespace llmq

#endif // hemis_LLMQ_BLOCKPROCESSOR_H
