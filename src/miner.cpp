// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h" // needed in case of no ENABLE_WALLET
#include "consensus/params.h"
#include "consensus/upgrades.h"
#include "hash.h"
#include "masternode-sync.h"
#include "net.h"
#include "pow.h"
#include "policy/feerate.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif
#include "validationinterface.h"
#include "masternode-payments.h"
#include "blocksignature.h"
#include "spork.h"
#include "invalid.h"
#include "policy/policy.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>


//////////////////////////////////////////////////////////////////////////////
//
// PIVXMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    std::set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) {}

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee) {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        } else {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (Params().GetConsensus().fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
}

bool CreateCoinbaseTx(CBlock* pblock, const CScript& scriptPubKeyIn, CBlockIndex* pindexPrev)
{
    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    //Masternode and general budget payments
    FillBlockPayee(txNew, pindexPrev ? pindexPrev->nHeight + 1 : 0, false);

    txNew.vin[0].scriptSig = CScript() << pindexPrev->nHeight + 1 << OP_0;
    // If no payee was detected, then the whole block value goes to the first output.
    if (txNew.vout.size() == 1) {
        txNew.vout[0].nValue = GetBlockValue(pindexPrev->nHeight + 1);
    }

    pblock->vtx.emplace_back(
            std::make_shared<const CTransaction>(CTransaction(txNew)));
    return true;
}

bool SolveProofOfStake(CBlock* pblock, CBlockIndex* pindexPrev, CWallet* pwallet, std::vector<CStakeableOutput>* availableCoins)
{
    boost::this_thread::interruption_point();
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
    CMutableTransaction txCoinStake;
    int64_t nTxNewTime = 0;
    if (!pwallet->CreateCoinStake(*pwallet, pindexPrev, pblock->nBits, txCoinStake, nTxNewTime, availableCoins)) {
        LogPrint(BCLog::STAKING, "%s : stake not found\n", __func__);
        return false;
    }
    // Stake found
    pblock->nTime = nTxNewTime;

    CMutableTransaction emptyTx;
    emptyTx.vout.emplace_back();
    emptyTx.vout[0].SetEmpty();
    emptyTx.vin.emplace_back();
    emptyTx.vin[0].scriptSig = CScript() << pindexPrev->nHeight + 1 << OP_0;
    pblock->vtx.emplace_back(
            std::make_shared<const CTransaction>(emptyTx));
    // stake
    pblock->vtx.emplace_back(
            std::make_shared<const CTransaction>(txCoinStake));
    return true;
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake, std::vector<CStakeableOutput>* availableCoins)
{
    // Create new block
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get()) return nullptr;
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    // Tip
    CBlockIndex* pindexPrev = GetChainTip();
    if (!pindexPrev) return nullptr;
    const int nHeight = pindexPrev->nHeight + 1;

    // Make sure to create the correct block version
    const Consensus::Params& consensus = Params().GetConsensus();

    bool fSaplingActive = NetworkUpgradeActive(nHeight, consensus, Consensus::UPGRADE_V5_0);

    if (fSaplingActive) {
        //!> Block v8: Sapling / tx v2
        pblock->nVersion = CBlockHeader::CURRENT_VERSION;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V4_0)) {
        pblock->nVersion = 7;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V3_4)) {
        pblock->nVersion = 6;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BIP65)) {
        pblock->nVersion = 5;
    } else if (consensus.NetworkUpgradeActive(nHeight, Consensus::UPGRADE_ZC)) {
        pblock->nVersion = 4;
    } else {
        pblock->nVersion = 3;
    }
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

    pblocktemplate->vTxFees.push_back(-1);   // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = gArgs.GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    unsigned int nBlockMaxSizeNetwork = MAX_BLOCK_SIZE_CURRENT;
    nBlockMaxSize = std::max((unsigned int)1000, std::min((nBlockMaxSizeNetwork - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = gArgs.GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = gArgs.GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        std::list<COrphan> vOrphan; // list memory doesn't move
        std::map<uint256, std::vector<COrphan*> > mapDependers;
        bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);

        // This vector will be sorted into a priority queue:
        std::vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (auto mi = mempool.mapTx.begin(); mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->GetTx();

            // Legacy zerocoin transactions are disabled
            assert(!tx.ContainsZerocoins());

            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight)){
                continue;
            }
            if(sporkManager.IsSporkActive(SPORK_20_SAPLING_MAINTENANCE) && tx.IsShieldTx()){
                continue;
            }

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;

            for (const CTxIn& txin : tx.vin) {
                // Read prev transaction
                if (!view.HaveCoin(txin.prevout)) {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash)) {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan) {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx.find(txin.prevout.hash)->GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }

                const Coin& coin = view.AccessCoin(txin.prevout);
                assert(!coin.IsSpent());

                CAmount nValueIn = coin.out.nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coin.nHeight;
                dPriority = double_safe_addition(dPriority, ((double)nValueIn * nConf));
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn - tx.GetValueOut(), nTxSize);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            } else
                vecPriority.emplace_back(dPriority, feeRate, &mi->GetTx());
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        // Keep track of block space used for shield txes
        unsigned int nSizeShield = 0;

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        std::vector<CBigNum> vBlockSerials;
        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = tx.GetTotalSize();
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            const bool isShield = tx.IsShieldTx();
            if (isShield && nSizeShield + nTxSize > MAX_BLOCK_SHIELD_TXES_SIZE)
                continue;

            // Legacy limits on sigOps:
            unsigned int nMaxBlockSigOps = MAX_BLOCK_SIGOPS_CURRENT;
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority))) {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(tx) - tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.

            CValidationState state;
            PrecomputedTransactionData precomTxData(tx);
            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, precomTxData))
                continue;

            UpdateCoins(tx, view, nHeight);

            // Added
            pblock->vtx.push_back(std::make_shared<const CTransaction>(tx));
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;
            if (isShield) nSizeShield += nTxSize;

            if (fPrintPriority) {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash)) {
                for (COrphan* porphan : mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty()) {
                            vecPriority.emplace_back(porphan->dPriority, porphan->feeRate, porphan->ptx);
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        if (!fProofOfStake) {
            // Coinbase can get the fees.
            CMutableTransaction txCoinbase(*pblock->vtx[0]);
            txCoinbase.vout[0].nValue += nFees;
            pblock->vtx[0] = std::make_shared<const CTransaction>(txCoinbase);
            pblocktemplate->vTxFees[0] = -nFees;
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("%s : total size %u\n", __func__, nBlockSize);

        // Sapling
        if (fSaplingActive) {
            SaplingMerkleTree sapling_tree;
            assert(view.GetSaplingAnchorAt(view.GetBestAnchor(), sapling_tree));

            // Update the Sapling commitment tree.
            for (const auto &tx : pblock->vtx) {
                if (tx->IsShieldTx()) {
                    for (const OutputDescription &odesc : tx->sapData->vShieldOutput) {
                        sapling_tree.append(odesc.cmu);
                    }
                }
            }
            // Update header
            pblock->hashFinalSaplingRoot = sapling_tree.root();
        }

        // Fill in header
        pblock->hashPrevBlock = pindexPrev->GetBlockHash();
        if (!fProofOfStake)
            UpdateTime(pblock, pindexPrev);
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce = 0;

        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(*(pblock->vtx[0]));

        if (fProofOfStake) {
            pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
            LogPrintf("CPUMiner : proof-of-stake block found %s \n", pblock->GetHash().GetHex());
            if (!SignBlock(*pblock, *pwallet)) {
                LogPrintf("%s: Signing new block with UTXO key failed \n", __func__);
                return nullptr;
            }
        }

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false)) {
            LogPrintf("CreateNewBlock() : TestBlockValidity failed\n");
            mempool.clear();
            return nullptr;
        }
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(std::shared_ptr<CBlock>& pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = std::make_shared<const CTransaction>(txCoinbase);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return nullptr;

    const int nHeightNext = chainActive.Tip()->nHeight + 1;

    // If we're building a late PoW block, don't continue
    // PoS blocks are built directly with CreateNewBlock
    if (Params().GetConsensus().NetworkUpgradeActive(nHeightNext, Consensus::UPGRADE_POS)) {
        LogPrintf("%s: Aborting PoW block creation during PoS phase\n", __func__);
        // sleep 1/2 a block time so we don't go into a tight loop.
        MilliSleep((Params().GetConsensus().nTargetSpacing * 1000) >> 1);
        return nullptr;
    }

    CScript scriptPubKey = GetScriptForDestination(pubkey.GetID());
    return CreateNewBlock(scriptPubKey, pwallet, false);
}

bool ProcessBlockFound(const std::shared_ptr<const CBlock>& pblock, CWallet& wallet, Optional<CReserveKey>& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0]->vout[0].nValue));

    // Found a solution
    {
        WAIT_LOCK(g_best_block_mutex, lock);
        if (pblock->hashPrevBlock != g_best_block)
            return error("PIVXMiner : generated block is stale");
    }

    // Remove key from key pool
    if (reservekey)
        reservekey->KeepKey();

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, nullptr, pblock, nullptr)) {
        return error("PIVXMiner : ProcessNewBlock, block not accepted");
    }

    g_connman->ForEachNode([&pblock](CNode* node)
    {
        node->PushInventory(CInv(MSG_BLOCK, pblock->GetHash()));
    });

    return true;
}

bool fGenerateBitcoins = false;
bool fStakeableCoins = false;

void CheckForCoins(CWallet* pwallet, std::vector<CStakeableOutput>* availableCoins)
{
    if (!pwallet || !pwallet->pStakerStatus)
        return;

    // control the amount of times the client will check for mintable coins (every block)
    {
        WAIT_LOCK(g_best_block_mutex, lock);
        if (g_best_block == pwallet->pStakerStatus->GetLastHash())
            return;
    }
    fStakeableCoins = pwallet->StakeableCoins(availableCoins);
}

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake)
{
    LogPrintf("PIVXMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    util::ThreadRename("pivx-miner");
    const Consensus::Params& consensus = Params().GetConsensus();
    const int64_t nSpacingMillis = consensus.nTargetSpacing * 1000;

    // Each thread has its own key and counter
    Optional<CReserveKey> opReservekey{nullopt};
    if (!fProofOfStake) {
        opReservekey = CReserveKey(pwallet);
    }

    // Available UTXO set
    std::vector<CStakeableOutput> availableCoins;
    unsigned int nExtraNonce = 0;

    while (fGenerateBitcoins || fProofOfStake) {
        CBlockIndex* pindexPrev = GetChainTip();
        if (!pindexPrev) {
            MilliSleep(nSpacingMillis);       // sleep a block
            continue;
        }
        if (fProofOfStake) {
            if (!consensus.NetworkUpgradeActive(pindexPrev->nHeight + 1, Consensus::UPGRADE_POS)) {
                // The last PoW block hasn't even been mined yet.
                MilliSleep(nSpacingMillis);       // sleep a block
                continue;
            }

            // update fStakeableCoins
            CheckForCoins(pwallet, &availableCoins);

            while ((g_connman && g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0 && Params().MiningRequiresPeers())
                    || pwallet->IsLocked() || !fStakeableCoins || masternodeSync.NotCompleted()) {
                MilliSleep(5000);
                // Do another check here to ensure fStakeableCoins is updated
                if (!fStakeableCoins) CheckForCoins(pwallet, &availableCoins);
            }

            //search our map of hashed blocks, see if bestblock has been hashed yet
            if (pwallet->pStakerStatus &&
                    pwallet->pStakerStatus->GetLastHash() == pindexPrev->GetBlockHash() &&
                    pwallet->pStakerStatus->GetLastTime() >= GetCurrentTimeSlot()) {
                MilliSleep(2000);
                continue;
            }

        } else if (pindexPrev->nHeight > 6 && consensus.NetworkUpgradeActive(pindexPrev->nHeight - 6, Consensus::UPGRADE_POS)) {
            // Late PoW: run for a little while longer, just in case there is a rewind on the chain.
            LogPrintf("%s: Exiting PoW Mining Thread at height: %d\n", __func__, pindexPrev->nHeight);
            return;
       }

        //
        // Create new block
        //
        unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();

        std::unique_ptr<CBlockTemplate> pblocktemplate((fProofOfStake ?
                                                        CreateNewBlock(CScript(), pwallet, true, &availableCoins) :
                                                        CreateNewBlockWithKey(*opReservekey, pwallet)));
        if (!pblocktemplate.get()) continue;
        std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);

        // POS - block found: process it
        if (fProofOfStake) {
            LogPrintf("%s : proof-of-stake block was signed %s \n", __func__, pblock->GetHash().ToString().c_str());
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            if (!ProcessBlockFound(pblock, *pwallet, opReservekey)) {
                LogPrintf("%s: New block orphaned\n", __func__);
                continue;
            }
            SetThreadPriority(THREAD_PRIORITY_LOWEST);
            continue;
        }

        // POW - miner main
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

        LogPrintf("Running PIVXMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
            ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

        //
        // Search
        //
        int64_t nStart = GetTime();
        uint256 hashTarget = uint256().SetCompact(pblock->nBits);
        while (true) {
            unsigned int nHashesDone = 0;

            uint256 hash;
            while (true) {
                hash = pblock->GetHash();
                if (hash <= hashTarget) {
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("%s:\n", __func__);
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                    ProcessBlockFound(pblock, *pwallet, opReservekey);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);

                    // In regression test mode, stop mining after a block is found. This
                    // allows developers to controllably generate a block on demand.
                    if (Params().IsRegTestNet())
                        throw boost::thread_interrupted();

                    break;
                }
                pblock->nNonce += 1;
                nHashesDone += 1;
                if ((pblock->nNonce & 0xFF) == 0)
                    break;
            }

            // Meter hashes/sec
            static int64_t nHashCounter;
            if (nHPSTimerStart == 0) {
                nHPSTimerStart = GetTimeMillis();
                nHashCounter = 0;
            } else
                nHashCounter += nHashesDone;
            if (GetTimeMillis() - nHPSTimerStart > 4000) {
                static RecursiveMutex cs;
                {
                    LOCK(cs);
                    if (GetTimeMillis() - nHPSTimerStart > 4000) {
                        dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = GetTimeMillis();
                        nHashCounter = 0;
                        static int64_t nLogTime;
                        if (GetTime() - nLogTime > 30 * 60) {
                            nLogTime = GetTime();
                            LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec / 1000.0);
                        }
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();
            if (    (g_connman && g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0 && Params().MiningRequiresPeers()) || // Regtest mode doesn't require peers
                    (pblock->nNonce >= 0xffff0000) ||
                    (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60) ||
                    (pindexPrev != chainActive.Tip())
                ) break;

            // Update nTime every few seconds
            UpdateTime(pblock.get(), pindexPrev);
            if (Params().GetConsensus().fPowAllowMinDifficultyBlocks) {
                // Changing pblock->nTime can change work required on testnet:
                hashTarget.SetCompact(pblock->nBits);
            }

        }
    }
}

void static ThreadBitcoinMiner(void* parg)
{
    boost::this_thread::interruption_point();
    CWallet* pwallet = (CWallet*)parg;
    try {
        BitcoinMiner(pwallet, false);
        boost::this_thread::interruption_point();
    } catch (const std::exception& e) {
        LogPrintf("PIVXMiner exception");
    } catch (...) {
        LogPrintf("PIVXMiner exception");
    }

    LogPrintf("PIVXMiner exiting\n");
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;
    fGenerateBitcoins = fGenerate;

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&ThreadBitcoinMiner, pwallet));
}

// ppcoin: stake minter thread
void ThreadStakeMinter()
{
    boost::this_thread::interruption_point();
    LogPrintf("ThreadStakeMinter started\n");
    CWallet* pwallet = pwalletMain;
    try {
        BitcoinMiner(pwallet, true);
        boost::this_thread::interruption_point();
    } catch (const std::exception& e) {
        LogPrintf("ThreadStakeMinter() exception \n");
    } catch (...) {
        LogPrintf("ThreadStakeMinter() error \n");
    }
    LogPrintf("ThreadStakeMinter exiting,\n");
}

#endif // ENABLE_WALLET
