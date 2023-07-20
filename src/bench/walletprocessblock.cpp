// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "blockassembler.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "evo/deterministicmns.h"
#include "evo/evodb.h"
#include "evo/evonotificationinterface.h"

#include "sapling/sapling_operation.h"
#include "scheduler.h"
#include "script/sigcache.h"
#include "sporkdb.h"
#include "txdb.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/thread.hpp>

// Number of blocks that will be created to make noise on this benchmark
const unsigned int CREATE_BLOCK = 500;
// Number of transparent transactions that will be created to make noise on this benchmark.
const unsigned int CREATE_TRANSACTIONS_PER_BLOCK = 20;

static CMutableTransaction NewCoinbase(const int nHeight, const CScript& scriptPubKey)
{
    CMutableTransaction txCoinbase;
    txCoinbase.vout.emplace_back();
    txCoinbase.vout[0].SetEmpty();
    txCoinbase.vout[0].scriptPubKey = scriptPubKey;
    txCoinbase.vin.emplace_back();
    txCoinbase.vin[0].scriptSig = CScript() << nHeight << OP_0;
    txCoinbase.vout[0].nValue = GetBlockValue(nHeight);
    return txCoinbase;
}

std::shared_ptr<CBlock> createAndProcessBlock(
        const CChainParams& params,
        const CScript& coinbaseScript,
        const std::vector<CMutableTransaction>& txns,
        CBlockIndex* prevpindex)
{
    int nextHeight = prevpindex ? prevpindex->nHeight + 1 : 0;
    CMutableTransaction coinbaseTx = NewCoinbase(nextHeight, coinbaseScript);

    CBlock block;
    block.hashMerkleRoot = BlockMerkleRoot(block);
    if (prevpindex) block.hashPrevBlock = prevpindex->GetBlockHash();
    block.vtx.emplace_back(MakeTransactionRef(coinbaseTx));
    for (const CMutableTransaction& tx : txns) {
        block.vtx.emplace_back(MakeTransactionRef(tx));
    }
    block.hashFinalSaplingRoot = CalculateSaplingTreeRoot(&block, nextHeight, params);

    const auto& blockHash = block.GetHash();
    CBlockIndex* fakeIndex = new CBlockIndex{block};
    fakeIndex->nHeight = nextHeight;
    BlockMap::iterator mi = mapBlockIndex.emplace(blockHash, fakeIndex).first;
    fakeIndex->phashBlock = &((*mi).first);
    chainActive.SetTip(fakeIndex);
    assert(chainActive.Contains(fakeIndex));
    assert(nextHeight == chainActive.Height());

    return std::make_shared<CBlock>(block);
}

CTransactionRef createNoisyTx(CWallet* pwallet, int numOfOutputs)
{
    CallResult<CTxDestination> ret = pwallet->getNewAddress("");
    assert(ret);
    const auto& dest = GetScriptForDestination(*ret.getObjResult());
    std::vector<CRecipient> vecSend;
    vecSend.reserve(numOfOutputs);
    for (int i=0; i<numOfOutputs; i++) {
        vecSend.emplace_back(dest, 5 * COIN, false);
    }

    CTransactionRef txRet;
    CReserveKey reservedKey(pwallet);
    CAmount nFeeRequired = 0;
    int nChangePosInOut = -1;
    std::string strFailReason;
    bool fCreated = pwallet->CreateTransaction(vecSend,
                                               txRet,
                                               reservedKey,
                                               nFeeRequired,
                                               nChangePosInOut,
                                               strFailReason);
    assert(fCreated);
    return txRet;
}

boost::thread_group threadGroup;
CScheduler scheduler;
EvoNotificationInterface* pEvoNotificationInterface;

static void initBasics()
{
    initZKSNARKS();
    CScheduler::Function serviceLoop = std::bind(&CScheduler::serviceQueue, &scheduler);
    threadGroup.create_thread(std::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));
    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

    zerocoinDB.reset(new CZerocoinDB(0, true));
    pSporkDB.reset(new CSporkDB(0, true));
    pblocktree.reset(new CBlockTreeDB(1 << 20, true));
    pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
    pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));
    evoDb.reset(new CEvoDB(1 << 20, true, true));
    deterministicMNManager.reset(new CDeterministicMNManager(*evoDb));
    pEvoNotificationInterface = new EvoNotificationInterface();
    RegisterValidationInterface(pEvoNotificationInterface);
}

static void WalletProcessBlockBench(benchmark::State& state)
{
    const static auto verify_handle = std::make_unique<ECCVerifyHandle>();

    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_POS, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_POS_V2, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_0, 1);
    const auto& params = Params();
    initBasics();

    std::unique_ptr<CWallet> pwallet = std::make_unique<CWallet>("default", WalletDatabase::CreateMock());
    bool isInit;
    pwallet->LoadWallet(isInit);
    pwallet->SetupSPKM(true, true);
    auto res = pwallet->getNewAddress("coinbase");
    if (!res) throw std::runtime_error("Cannot create coinbase address");
    const CScript& coinbaseScript = GetScriptForDestination(*res.getObjResult());

    // Let's generate blocks first
    int gen = CREATE_BLOCK;
    for (int i=0; i<gen; i++) {
        std::vector<CMutableTransaction> vtx;
        if (i > 101) { // Generate extra transaction
            vtx.emplace_back(*createNoisyTx(pwallet.get(), 15));
            if (i > 106) {
                for (int j=0; j < (int)CREATE_TRANSACTIONS_PER_BLOCK; j++) {
                    vtx.emplace_back(*createNoisyTx(pwallet.get(), 1));
                }
            }
        }

        std::shared_ptr<CBlock> pblock = createAndProcessBlock(params, coinbaseScript, vtx, chainActive.Tip());
        pwallet->BlockConnected(pblock, mapBlockIndex[pblock->GetHash()]);
    }
    assert(WITH_LOCK(cs_main, return chainActive.Height();) == gen -1);
    int nextBlockHeight = gen + 1;

    // Now the wallet has balance to shield and the network is mature enough.
    // let's generate 30 more blocks with 3 shielding transactions each.
    std::vector<std::shared_ptr<CBlock>> blocks;
    for (int i=0; i<30; i++) {
        std::vector<CMutableTransaction> vtx;
        for (int j=0; j<3; j++) {
            std::vector<SendManyRecipient> recipients;
            recipients.emplace_back(pwallet->GenerateNewSaplingZKey(), 2 * COIN, "", false);
            recipients.emplace_back(pwallet->GenerateNewSaplingZKey(), 100 * COIN, "", false);
            recipients.emplace_back(pwallet->GenerateNewSaplingZKey(), 240 * COIN, "", false);

            SaplingOperation operation(params.GetConsensus(), pwallet.get());
            auto operationResult = operation.setRecipients(recipients)
                    ->setSelectTransparentCoins(true)
                    ->setMinDepth(1)
                    ->build();

            assert(operationResult);
            vtx.emplace_back(operation.getFinalTx());
        }

        std::shared_ptr<CBlock> pblock = createAndProcessBlock(params,
                                                               coinbaseScript,
                                                               vtx,
                                                               chainActive.Tip());
        assert(WITH_LOCK(cs_main, return chainActive.Height();) == nextBlockHeight - 1);
        blocks.emplace_back(pblock);
        nextBlockHeight++;
    }

    // The wallet receiving the blocks..
    while (state.KeepRunning()) {
        for (const auto& pblock : blocks) {
            pwallet->BlockConnected(pblock, mapBlockIndex[pblock->GetHash()]);
        }
    }

    // Cleanup
    ECC_Stop();
    deterministicMNManager.reset();
    evoDb.reset();
    scheduler.stop();
    threadGroup.interrupt_all();
    threadGroup.join_all();
    UnloadBlockIndex();
    chainActive.SetTip(nullptr);
    delete pEvoNotificationInterface;
    pcoinsTip.reset();
    pcoinsdbview.reset();
    pblocktree.reset();
    zerocoinDB.reset();
    pSporkDB.reset();
}

BENCHMARK(WalletProcessBlockBench, 0);
