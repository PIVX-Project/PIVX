// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "blockassembler.h"

#include "amount.h"
#include "blocksignature.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "masternode-payments.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "spork.h"
#include "timedata.h"
#include "util/system.h"
#include "validation.h"
#include "validationinterface.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>
#include <queue>

// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

class ScoreCompare
{
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b,*a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);

    return nNewTime - nOldTime;
}

static CMutableTransaction NewCoinbase(const int nHeight, const CScript* pScriptPubKey = nullptr)
{
    CMutableTransaction txCoinbase;
    txCoinbase.vout.emplace_back();
    txCoinbase.vout[0].SetEmpty();
    if (pScriptPubKey) txCoinbase.vout[0].scriptPubKey = *pScriptPubKey;
    txCoinbase.vin.emplace_back();
    txCoinbase.vin[0].scriptSig = CScript() << nHeight << OP_0;
    return txCoinbase;
}

bool SolveProofOfStake(CBlock* pblock, CBlockIndex* pindexPrev, CWallet* pwallet, std::vector<CStakeableOutput>* availableCoins)
{
    boost::this_thread::interruption_point();

    assert(pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);

    // Sync wallet before create coinstake
    pwallet->BlockUntilSyncedToCurrentChain();

    CMutableTransaction txCoinStake;
    int64_t nTxNewTime = 0;
    if (!pwallet->CreateCoinStake(pindexPrev, pblock->nBits, txCoinStake, nTxNewTime, availableCoins)) {
        LogPrint(BCLog::STAKING, "%s : stake not found\n", __func__);
        return false;
    }
    // Stake found

    // Create coinbase tx and add masternode/budget payments
    CMutableTransaction txCoinbase = NewCoinbase(pindexPrev->nHeight + 1);
    FillBlockPayee(txCoinbase, txCoinStake, pindexPrev, true);

    // Sign coinstake
    if (!pwallet->SignCoinStake(txCoinStake)) {
        const COutPoint& stakeIn = txCoinStake.vin[0].prevout;
        return error("Unable to sign coinstake with input %s-%d", stakeIn.hash.ToString(), stakeIn.n);
    }

    pblock->vtx.emplace_back(MakeTransactionRef(txCoinbase));
    pblock->vtx.emplace_back(MakeTransactionRef(txCoinStake));
    pblock->nTime = nTxNewTime;
    return true;
}

CMutableTransaction CreateCoinbaseTx(const CScript& scriptPubKeyIn, CBlockIndex* pindexPrev)
{
    assert(pindexPrev);
    const int nHeight = pindexPrev->nHeight + 1;

    // Create coinbase tx
    CMutableTransaction txCoinbase = NewCoinbase(nHeight, &scriptPubKeyIn);

    //Masternode and general budget payments
    CMutableTransaction txDummy;    // POW blocks have no coinstake
    FillBlockPayee(txCoinbase, txDummy, pindexPrev, false);

    // If no payee was detected, then the whole block value goes to the first output.
    if (txCoinbase.vout.size() == 1) {
        txCoinbase.vout[0].nValue = GetBlockValue(nHeight);
    }

    return txCoinbase;
}

bool CreateCoinbaseTx(CBlock* pblock, const CScript& scriptPubKeyIn, CBlockIndex* pindexPrev)
{
    pblock->vtx.emplace_back(MakeTransactionRef(CreateCoinbaseTx(scriptPubKeyIn, pindexPrev)));
    return true;
}

BlockAssembler::BlockAssembler(const CChainParams& _chainparams, const bool _defaultPrintPriority)
        : chainparams(_chainparams), defaultPrintPriority(_defaultPrintPriority)
{
    // Largest block you're willing to create:
    nBlockMaxSize = gArgs.GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to between 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE_CURRENT - 1000), nBlockMaxSize));

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    nBlockMinSize = gArgs.GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockSigOps = 100;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;

    lastFewTxs = 0;
    blockFinished = false;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn,
                                               CWallet* pwallet,
                                               bool fProofOfStake,
                                               std::vector<CStakeableOutput>* availableCoins,
                                               bool fNoMempoolTx)
{
    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate) return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    CBlockIndex* pindexPrev = WITH_LOCK(cs_main, return chainActive.Tip());
    assert(pindexPrev);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(chainparams.GetConsensus(), nHeight);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().IsRegTestNet()) {
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);
    }

    // Depending on the tip height, try to find a coinstake who solves the block or create a coinbase tx.
    if (!(fProofOfStake ? SolveProofOfStake(pblock, pindexPrev, pwallet, availableCoins)
                        : CreateCoinbaseTx(pblock, scriptPubKeyIn, pindexPrev))) {
        return nullptr;
    }

    if (!fNoMempoolTx) {
        // Add transactions from mempool
        LOCK2(cs_main,mempool.cs);
        addPriorityTxs();
        addScoreTxs();
    }

    if (!fProofOfStake) {
        // Coinbase can get the fees.
        CMutableTransaction txCoinbase(*pblock->vtx[0]);
        txCoinbase.vout[0].nValue += nFees;
        pblock->vtx[0] = MakeTransactionRef(txCoinbase);
        pblocktemplate->vTxFees[0] = -nFees;
    }

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOps);

    const Consensus::Params& consensus = Params().GetConsensus();
    // Fill in header
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    if (!fProofOfStake) UpdateTime(pblock, consensus, pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
    pblock->nNonce = 0;
    pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(*(pblock->vtx[0]));
    appendSaplingTreeRoot();

    if (fProofOfStake) { // this is only for PoS because the IncrementExtraNonce does it for PoW
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        LogPrintf("CPUMiner : proof-of-stake block found %s \n", pblock->GetHash().GetHex());
        if (!SignBlock(*pblock, *pwallet)) {
            LogPrintf("%s: Signing new block with UTXO key failed \n", __func__);
            return nullptr;
        }
    }

    {
        LOCK(cs_main);
        if (chainActive.Tip() != pindexPrev) return nullptr; // new block came in, move on

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false, false)) {
            throw std::runtime_error(
                    strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
        }
    }

    return std::move(pblocktemplate);
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter)
{
    for (CTxMemPool::txiter parent : mempool.GetMemPoolParents(iter)) {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

bool BlockAssembler::TestForBlock(CTxMemPool::txiter iter)
{
    // Legacy zerocoin transactions are disabled forever.
    assert(!iter->GetSharedTx()->ContainsZerocoins());

    // Don't add shielded transactions if Sapling isn't active
    const bool isShielded = iter->IsShielded();
    if(isShielded && sporkManager.IsSporkActive(SPORK_20_SAPLING_MAINTENANCE)) {
        return false;
    }

    unsigned int nTxSize = iter->GetTxSize();
    if (nBlockSize + nTxSize >= nBlockMaxSize) {
        // If the block is so close to full that no more txs will fit
        // or if we've tried more than 50 times to fill remaining space
        // then flag that the block is finished
        if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
            blockFinished = true;
            return false;
        }
        // Once we're within 1000 bytes of a full block, only look at 50 more txs
        // to try to fill the remaining space.
        if (nBlockSize > nBlockMaxSize - 1000) {
            lastFewTxs++;
        }
        return false;
    }

    if (isShielded && nSizeShielded + nTxSize > MAX_BLOCK_SHIELDED_TXES_SIZE) {
        return false;
    }

    if (nBlockSigOps + iter->GetSigOpCount() >= MAX_BLOCK_SIGOPS_CURRENT) {
        // If the block has room for no more sig ops then
        // flag that the block is finished
        if (nBlockSigOps > MAX_BLOCK_SIGOPS_CURRENT - 2) {
            blockFinished = true;
            return false;
        }
        // Otherwise attempt to find another tx with fewer sigops
        // to put in the block.
        return false;
    }

    // Must check that lock times are still valid
    // This can be removed once MTP is always enforced
    // as long as reorgs keep the mempool consistent.
    if (!IsFinalTx(iter->GetSharedTx(), nHeight))
        return false;

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOps.push_back(iter->GetSigOpCount());
    nBlockSize += iter->GetTxSize();
    ++nBlockTx;
    nBlockSigOps += iter->GetSigOpCount();
    nFees += iter->GetFee();
    if (iter->IsShielded()) nSizeShielded += iter->GetTxSize();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", defaultPrintPriority);
    if (fPrintPriority) {
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy{0};
        mempool.ApplyDeltas(iter->GetTx().GetHash(), dPriority, dummy);
        LogPrintf("priority %.1f fee %s txid %s\n",
                  dPriority,
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

void BlockAssembler::addScoreTxs()
{
    std::priority_queue<CTxMemPool::txiter, std::vector<CTxMemPool::txiter>, ScoreCompare> clearedTxs;
    CTxMemPool::setEntries waitSet;
    CTxMemPool::indexed_transaction_set::index<mining_score>::type::iterator mi = mempool.mapTx.get<mining_score>().begin();
    CTxMemPool::txiter iter;
    while (!blockFinished && (mi != mempool.mapTx.get<mining_score>().end() || !clearedTxs.empty())) {
        // If no txs that were previously postponed are available to try
        // again, then try the next highest score tx
        if (clearedTxs.empty()) {
            iter = mempool.mapTx.project<0>(mi);
            mi++;
        }
        // If a previously postponed tx is available to try again, then it
        // has higher score than all untried so far txs
        else {
            iter = clearedTxs.top();
            clearedTxs.pop();
        }

        // If tx already in block, skip  (added by addPriorityTxs)
        if (inBlock.count(iter)) {
            continue;
        }

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitSet.insert(iter);
            continue;
        }

        // If the fee rate is below the min fee rate for mining, then we're done
        // adding txs based on score (fee rate)
        if (iter->GetModifiedFee() < ::minRelayTxFee.GetFee(iter->GetTxSize()) && nBlockSize >= nBlockMinSize) {
            return;
        }

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // This tx was successfully added, so
            // add transactions that depend on this one to the priority queue to try again
            for (CTxMemPool::txiter child : mempool.GetMemPoolChildren(iter)) {
                if (waitSet.count(child)) {
                    clearedTxs.push(child);
                    waitSet.erase(child);
                }
            }
        }
    }
}

void BlockAssembler::addPriorityTxs()
{
    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = gArgs.GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    if (nBlockPrioritySize == 0) {
        return;
    }

    // This vector will be sorted into a priority queue:
    std::vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi) {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy{0};
        mempool.ApplyDeltas(mi->GetSharedTx()->GetHash(), dPriority, dummy);
        vecPriority.emplace_back(TxCoinAgePriority(dPriority, mi));
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;
    while (!vecPriority.empty() && !blockFinished) { // add a tx from priority queue to fill the blockprioritysize
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        // If tx already in block, skip
        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen for priority txs
            continue;
        }

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // If now that this txs is added we've surpassed our desired priority size
            // or have dropped below the AllowFreeThreshold, then we're done adding priority txs
            if (nBlockSize >= nBlockPrioritySize || !AllowFree(actualPriority)) {
                return;
            }

            // This tx was successfully added, so
            // add transactions that depend on this one to the priority queue to try again
            for (CTxMemPool::txiter child : mempool.GetMemPoolChildren(iter)) {
                waitPriIter wpiter = waitPriMap.find(child);
                if (wpiter != waitPriMap.end()) {
                    vecPriority.emplace_back(TxCoinAgePriority(wpiter->second,child));
                    std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                    waitPriMap.erase(wpiter);
                }
            }
        }
    }
}

void BlockAssembler::appendSaplingTreeRoot()
{
    // Update header
    pblock->hashFinalSaplingRoot = CalculateSaplingTreeRoot(pblock, nHeight, chainparams);
}

uint256 CalculateSaplingTreeRoot(CBlock* pblock, int nHeight, const CChainParams& chainparams)
{
    if (NetworkUpgradeActive(nHeight, chainparams.GetConsensus(), Consensus::UPGRADE_V5_0)) {
        SaplingMerkleTree sapling_tree;
        assert(pcoinsTip->GetSaplingAnchorAt(pcoinsTip->GetBestAnchor(), sapling_tree));

        // Update the Sapling commitment tree.
        for (const auto &tx : pblock->vtx) {
            if (tx->IsShieldedTx()) {
                for (const OutputDescription &odesc : tx->sapData->vShieldedOutput) {
                    sapling_tree.append(odesc.cmu);
                }
            }
        }
        return sapling_tree.root();
    }
    return UINT256_ZERO;
}

bool SolveBlock(std::shared_ptr<CBlock>& pblock, int nHeight)
{
    unsigned int extraNonce = 0;
    IncrementExtraNonce(pblock, nHeight, extraNonce);
    while (pblock->nNonce < std::numeric_limits<uint32_t>::max() &&
           !CheckProofOfWork(pblock->GetHash(), pblock->nBits)) {
        ++pblock->nNonce;
    }
    return pblock->nNonce != std::numeric_limits<uint32_t>::max();
}

void IncrementExtraNonce(std::shared_ptr<CBlock>& pblock, int nHeight, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(txCoinbase);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

int32_t ComputeBlockVersion(const Consensus::Params& consensus, int nHeight)
{
    if (NetworkUpgradeActive(nHeight, consensus, Consensus::UPGRADE_V5_0)) {
        return CBlockHeader::CURRENT_VERSION;       // v10 (since 5.1.99)
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V4_0)) {
        return 7;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V3_4)) {
        return 6;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BIP65)) {
        return 5;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_ZC)) {
        return 4;
    } else {
        return 3;
    }
}

