// Copyright (c) 2020-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"

#include "blockassembler.h"
#include "coincontrol.h"
#include "util/blockstatecatcher.h"
#include "blocksignature.h"
#include "consensus/merkle.h"
#include "primitives/block.h"
#include "script/sign.h"
#include "test/util/blocksutil.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(pos_validations_tests)

void reSignTx(CMutableTransaction& mtx,
              const std::vector<CTxOut>& txPrevOutputs,
              CWallet* wallet)
{
    CTransaction txNewConst(mtx);
    for (int index=0; index < (int) txPrevOutputs.size(); index++) {
        const CTxOut& prevOut = txPrevOutputs.at(index);
        SignatureData sigdata;
        BOOST_ASSERT(ProduceSignature(
                TransactionSignatureCreator(wallet, &txNewConst, index, prevOut.nValue, SIGHASH_ALL),
                prevOut.scriptPubKey,
                sigdata,
                txNewConst.GetRequiredSigVersion(),
                true));
        UpdateTransaction(mtx, index, sigdata);
    }
}

BOOST_FIXTURE_TEST_CASE(coinstake_tests, TestPoSChainSetup)
{
    // Verify that we are at block 251
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->nHeight), 250);
    SyncWithValidationInterfaceQueue();

    // Let's create the block
    std::vector<CStakeableOutput> availableCoins;
    BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
            Params(), false).CreateNewBlock(CScript(),
                                            pwalletMain.get(),
                                            true,
                                            &availableCoins,
                                            true);
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
    BOOST_CHECK(pblock->IsProofOfStake());

    // Add a second input to a coinstake
    CMutableTransaction mtx(*pblock->vtx[1]);
    const CStakeableOutput& in2 = availableCoins.back();
    availableCoins.pop_back();
    CTxIn vin2(in2.tx->GetHash(), in2.i);
    mtx.vin.emplace_back(vin2);

    CTxOut prevOutput1 = pwalletMain->GetWalletTx(mtx.vin[0].prevout.hash)->tx->vout[mtx.vin[0].prevout.n];
    std::vector<CTxOut> txPrevOutputs{prevOutput1, in2.tx->tx->vout[in2.i]};

    reSignTx(mtx, txPrevOutputs, pwalletMain.get());
    pblock->vtx[1] = MakeTransactionRef(mtx);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    ProcessBlockAndCheckRejectionReason(pblock, "bad-cs-multi-inputs", 250);

    // Check multi-empty-outputs now
    pblock = std::make_shared<CBlock>(pblocktemplate->block);
    mtx = CMutableTransaction(*pblock->vtx[1]);
    for (int i = 0; i < 999; ++i) {
        mtx.vout.emplace_back();
        mtx.vout.back().SetEmpty();
    }
    reSignTx(mtx, {prevOutput1}, pwalletMain.get());
    pblock->vtx[1] = MakeTransactionRef(mtx);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    ProcessBlockAndCheckRejectionReason(pblock, "bad-txns-vout-empty", 250);

    // Now connect the proper block
    pblock = std::make_shared<CBlock>(pblocktemplate->block);
    ProcessNewBlock(pblock, nullptr);
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash()), pblock->GetHash());
}

CTransaction CreateAndCommitTx(CWallet* pwalletMain, const CTxDestination& dest, CAmount destValue, CCoinControl* coinControl = nullptr)
{
    CTransactionRef txNew;
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRet = 0;
    std::string strFailReason;
    // The minimum depth (100) required to spend coinbase outputs is calculated from the current chain tip.
    // Since this transaction could be included in a fork block, at a lower height, this may result in
    // selecting non-yet-available inputs, and thus creating non-connectable blocks due to premature-cb-spend.
    // So, to be sure, only select inputs which are more than 120-blocks deep in the chain.
    BOOST_ASSERT(pwalletMain->CreateTransaction(GetScriptForDestination(dest),
                                   destValue,
                                   txNew,
                                   reservekey,
                                   nFeeRet,
                                   strFailReason,
                                   coinControl,
                                   true, /* sign*/
                                   false, /*fIncludeDelegated*/
                                   nullptr, /*fStakeDelegationVoided*/
                                   0, /*nExtraSize*/
                                   120 /*nMinDepth*/));
    pwalletMain->CommitTransaction(txNew, reservekey, nullptr);
    return *txNew;
}

COutPoint GetOutpointWithAmount(const CTransaction& tx, CAmount outpointValue)
{
    for (size_t i = 0; i < tx.vout.size(); i++) {
        if (tx.vout[i].nValue == outpointValue) {
            return COutPoint(tx.GetHash(), i);
        }
    }
    BOOST_ASSERT_MSG(false, "error in test, no output in tx for value");
    return {};
}

static bool IsSpentOnFork(const COutput& coin, std::initializer_list<std::shared_ptr<CBlock>> forkchain = {})
{
    for (const auto& block : forkchain) {
        const auto& usedOutput = block->vtx[1]->vin.at(0).prevout;
        if (coin.tx->GetHash() == usedOutput.hash && coin.i == (int)usedOutput.n) {
            // spent on fork
            return true;
        }
    }
    return false;
}

std::shared_ptr<CBlock> CreateBlockInternal(CWallet* pwalletMain, const std::vector<CMutableTransaction>& txns = {},
                                            CBlockIndex* customPrevBlock = nullptr,
                                            std::initializer_list<std::shared_ptr<CBlock>> forkchain = {})
{
    std::vector<CStakeableOutput> availableCoins;
    BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));

    // Remove any utxo which is not deeper than 120 blocks (for the same reasoning
    // used when selecting tx inputs in CreateAndCommitTx)
    // Also, as the wallet is not prepared to follow several chains at the same time,
    // need to manually remove from the stakeable utxo set every already used
    // coinstake inputs on the previous blocks of the parallel chain so they
    // are not used again.
    for (auto it = availableCoins.begin(); it != availableCoins.end() ;) {
        if (it->nDepth <= 120 || IsSpentOnFork(*it, forkchain)) {
            it = availableCoins.erase(it);
        } else {
            it++;
        }
    }

    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
            Params(), false).CreateNewBlock(CScript(),
                                            pwalletMain,
                                            true,
                                            &availableCoins,
                                            true,
                                            false,
                                            customPrevBlock,
                                            false);
    BOOST_ASSERT(pblocktemplate);
    auto pblock = std::make_shared<CBlock>(pblocktemplate->block);
    if (!txns.empty()) {
        for (const auto& tx : txns) {
            pblock->vtx.emplace_back(MakeTransactionRef(tx));
        }
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        assert(SignBlock(*pblock, *pwalletMain));
    }
    return pblock;
}

static COutput GetUnspentCoin(CWallet* pwallet, std::initializer_list<std::shared_ptr<CBlock>> forkchain = {})
{
    std::vector<COutput> availableCoins;
    CWallet::AvailableCoinsFilter coinsFilter;
    coinsFilter.minDepth = 120;
    BOOST_CHECK(pwallet->AvailableCoins(&availableCoins, nullptr, coinsFilter));
    for (const auto& coin : availableCoins) {
        if (!IsSpentOnFork(coin, forkchain)) {
            return coin;
        }
    }
    throw std::runtime_error("Unspent coin not found");
}

BOOST_FIXTURE_TEST_CASE(created_on_fork_tests, TestPoSChainSetup)
{
    // Let's create few more PoS blocks
    for (int i=0; i<30; i++) {
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get());
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));
    }

    /*
    Chains diagram:
    A -- B -- C -- D -- E -- F -- G -- H -- I
              \
                -- D1 -- E1 -- F1
                     \
                      -- E2 -- F2
              \
                -- D3 -- E3 -- F3
                           \
                            -- F3_1 -- G3 -- H3 -- I3
    */

    // Tests:
    // 1) coins created in D1 and spent in E1. --> should pass
    // 2) coins created in E, being spent in D4 --> should fail.
    // 3) coins created and spent in E2, being double spent in F2. --> should fail
    // 4) coins created in D and spent in E3. --> should fail
    // 5) coins create in D, spent in F and then double spent in F3. --> should fail
    // 6) coins created in G and G3, being spent in H and H3 --> should pass.
    // 7) use coinstake on different chains --> should pass.

    // Let's create block C with a valid cTx
    auto cTx = CreateAndCommitTx(pwalletMain.get(), *pwalletMain->getNewAddress("").getObjResult(), 249 * COIN);
    const auto& cTx_out = GetOutpointWithAmount(cTx, 249 * COIN);
    WITH_LOCK(pwalletMain->cs_wallet, pwalletMain->LockCoin(cTx_out));
    std::shared_ptr<CBlock> pblockC = CreateBlockInternal(pwalletMain.get(), {cTx});
    BOOST_CHECK(ProcessNewBlock(pblockC, nullptr));

    // Create block D with a valid dTx
    auto dTx = CreateAndCommitTx(pwalletMain.get(), *pwalletMain->getNewAddress("").getObjResult(), 249 * COIN);
    auto dTxOutPoint = GetOutpointWithAmount(dTx, 249 * COIN);
    WITH_LOCK(pwalletMain->cs_wallet, pwalletMain->LockCoin(dTxOutPoint));
    std::shared_ptr<CBlock> pblockD = CreateBlockInternal(pwalletMain.get(), {dTx});

    // Create D1 forked block that connects a new tx
    auto dest = pwalletMain->getNewAddress("").getObjResult();
    auto d1Tx = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN);
    std::shared_ptr<CBlock> pblockD1 = CreateBlockInternal(pwalletMain.get(), {d1Tx});

    // Process blocks
    ProcessNewBlock(pblockD, nullptr);
    ProcessNewBlock(pblockD1, nullptr);
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() ==  pblockD->GetHash()));

    // Ensure that the coin does not exist in the main chain
    const Coin& utxo = pcoinsTip->AccessCoin(COutPoint(d1Tx.GetHash(), 0));
    BOOST_CHECK(utxo.out.IsNull());

    // Create valid block E
    auto eTx = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN);
    std::shared_ptr<CBlock> pblockE = CreateBlockInternal(pwalletMain.get(), {eTx});
    BOOST_CHECK(ProcessNewBlock(pblockE, nullptr));

    // #################################################
    // ### 1) -> coins created in D' and spent in E' ###
    // #################################################

    // Create tx spending the previously created tx on the forked chain
    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = false;
    coinControl.Select(COutPoint(d1Tx.GetHash(), 0), d1Tx.vout[0].nValue);
    auto e1Tx = CreateAndCommitTx(pwalletMain.get(), *dest, d1Tx.vout[0].nValue - 0.1 * COIN, &coinControl);

    CBlockIndex* pindexPrev = mapBlockIndex.at(pblockD1->GetHash());
    std::shared_ptr<CBlock> pblockE1 = CreateBlockInternal(pwalletMain.get(), {e1Tx}, pindexPrev, {pblockD1});
    BOOST_CHECK(ProcessNewBlock(pblockE1, nullptr));

    // #################################################################
    // ### 2) coins created in E, being spent in D4 --> should fail. ###
    // #################################################################

    coinControl.UnSelectAll();
    coinControl.Select(GetOutpointWithAmount(eTx, 200 * COIN), 200 * COIN);
    coinControl.fAllowOtherInputs = false;
    auto D4_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);
    std::shared_ptr<CBlock> pblockD4 = CreateBlockInternal(pwalletMain.get(), {D4_tx1}, mapBlockIndex.at(pblockC->GetHash()));
    BOOST_CHECK(!ProcessNewBlock(pblockD4, nullptr));

    // #####################################################################
    // ### 3) -> coins created and spent in E2, being double spent in F2 ###
    // #####################################################################

    // Create block E2 with E2_tx1 and E2_tx2. Where E2_tx2 is spending the outputs of E2_tx1
    CCoinControl coinControlE2;
    coinControlE2.Select(cTx_out, 249 * COIN);
    auto E2_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN, &coinControlE2);

    coinControl.UnSelectAll();
    coinControl.Select(GetOutpointWithAmount(E2_tx1, 200 * COIN), 200 * COIN);
    coinControl.fAllowOtherInputs = false;
    auto E2_tx2 = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);

    std::shared_ptr<CBlock> pblockE2 = CreateBlockInternal(pwalletMain.get(), {E2_tx1, E2_tx2},
                                                           pindexPrev, {pblockD1});
    BOOST_CHECK(ProcessNewBlock(pblockE2, nullptr));

    // Create block with F2_tx1 spending E2_tx1 again.
    auto F2_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);

    pindexPrev = mapBlockIndex.at(pblockE2->GetHash());
    std::shared_ptr<CBlock> pblock5Forked = CreateBlockInternal(pwalletMain.get(), {F2_tx1},
                                                                pindexPrev, {pblockD1, pblockE2});
    BlockStateCatcher stateCatcher(pblock5Forked->GetHash());
    stateCatcher.registerEvent();
    BOOST_CHECK(!ProcessNewBlock(pblock5Forked, nullptr));
    BOOST_CHECK(stateCatcher.found);
    BOOST_CHECK(!stateCatcher.state.IsValid());
    BOOST_CHECK_EQUAL(stateCatcher.state.GetRejectReason(), "bad-txns-inputs-spent-fork-post-split");

    // #############################################
    // ### 4) coins created in D and spent in E3 ###
    // #############################################

    // First create D3
    pindexPrev = mapBlockIndex.at(pblockC->GetHash());
    std::shared_ptr<CBlock> pblockD3 = CreateBlockInternal(pwalletMain.get(), {}, pindexPrev);
    BOOST_CHECK(ProcessNewBlock(pblockD3, nullptr));

    // Now let's try to spend the coins created in D in E3
    coinControl.UnSelectAll();
    coinControl.Select(dTxOutPoint, 249 * COIN);
    coinControl.fAllowOtherInputs = false;
    auto E3_tx1 = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN, &coinControl);

    pindexPrev = mapBlockIndex.at(pblockD3->GetHash());
    std::shared_ptr<CBlock> pblockE3 = CreateBlockInternal(pwalletMain.get(), {E3_tx1}, pindexPrev, {pblockD3});
    stateCatcher.clear();
    stateCatcher.setBlockHash(pblockE3->GetHash());
    BOOST_CHECK(!ProcessNewBlock(pblockE3, nullptr));
    BOOST_CHECK(stateCatcher.found);
    BOOST_CHECK(!stateCatcher.state.IsValid());
    BOOST_CHECK_EQUAL(stateCatcher.state.GetRejectReason(), "bad-txns-inputs-created-post-split");

    // ####################################################################
    // ### 5) coins create in D, spent in F and then double spent in F3 ###
    // ####################################################################

    // Create valid block F spending the coins created in D
    const auto& F_tx1 = E3_tx1;
    std::shared_ptr<CBlock> pblockF = CreateBlockInternal(pwalletMain.get(), {F_tx1});
    BOOST_CHECK(ProcessNewBlock(pblockF, nullptr));
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() ==  pblockF->GetHash()));

    // Create valid block E3
    pindexPrev = mapBlockIndex.at(pblockD3->GetHash());
    pblockE3 = CreateBlockInternal(pwalletMain.get(), {}, pindexPrev, {pblockD3});
    BOOST_CHECK(ProcessNewBlock(pblockE3, nullptr));

    // Now double spend F_tx1 in F3
    pindexPrev = mapBlockIndex.at(pblockE3->GetHash());
    std::shared_ptr<CBlock> pblockF3 = CreateBlockInternal(pwalletMain.get(), {F_tx1}, pindexPrev, {pblockD3, pblockE3});
    // Accepted on disk but not connected.
    BOOST_CHECK(ProcessNewBlock(pblockF3, nullptr));
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash();) != pblockF3->GetHash());

    {
        // Trigger a rescan so the wallet cleans up its internal state.
        WalletRescanReserver reserver(pwalletMain.get());
        BOOST_CHECK(reserver.reserve());
        pwalletMain->RescanFromTime(0, reserver, true /* update */);
    }

    // ##############################################################################
    // ### 6) coins created in G and G3, being spent in H and H3 --> should pass. ###
    // ##############################################################################

    // First create new coins in G
    // select an input that is not already spent in D3 or E3 (since we want to spend it also in G3)
    const COutput& input = GetUnspentCoin(pwalletMain.get(), {pblockD3, pblockE3});

    coinControl.UnSelectAll();
    coinControl.Select(COutPoint(input.tx->GetHash(), input.i), input.Value());
    coinControl.fAllowOtherInputs = false;

    dest = pwalletMain->getNewAddress("").getObjResult();
    auto gTx = CreateAndCommitTx(pwalletMain.get(), *dest, 200 * COIN, &coinControl);
    auto gOut = GetOutpointWithAmount(gTx, 200 * COIN);
    std::shared_ptr<CBlock> pblockG = CreateBlockInternal(pwalletMain.get(), {gTx});
    BOOST_CHECK(ProcessNewBlock(pblockG, nullptr));

    // Now create the same coin in G3
    pblockF3 = CreateBlockInternal(pwalletMain.get(), {}, mapBlockIndex.at(pblockE3->GetHash()), {pblockD3, pblockE3});
    BOOST_CHECK(ProcessNewBlock(pblockF3, nullptr));
    auto pblockG3 = CreateBlockInternal(pwalletMain.get(), {gTx}, mapBlockIndex.at(pblockF3->GetHash()), {pblockD3, pblockE3, pblockF3});
    BOOST_CHECK(ProcessNewBlock(pblockG3, nullptr));
    FlushStateToDisk();

    // Now spend the coin in both, H and H3
    coinControl.UnSelectAll();
    coinControl.Select(gOut, 200 * COIN);
    coinControl.fAllowOtherInputs = false;
    auto hTx = CreateAndCommitTx(pwalletMain.get(), *dest, 199 * COIN, &coinControl);
    std::shared_ptr<CBlock> pblockH = CreateBlockInternal(pwalletMain.get(), {hTx});
    BOOST_CHECK(ProcessNewBlock(pblockH, nullptr));
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() ==  pblockH->GetHash()));
    FlushStateToDisk();

    // H3 now..
    std::shared_ptr<CBlock> pblockH3 = CreateBlockInternal(pwalletMain.get(),
                                                           {hTx},
                                                           mapBlockIndex.at(pblockG3->GetHash()),
                                                           {pblockD3, pblockE3, pblockF3, pblockG3});
    BOOST_CHECK(ProcessNewBlock(pblockH3, nullptr));

    // Try to read the forking point manually
    CBlock bl;
    BOOST_CHECK(ReadBlockFromDisk(bl, mapBlockIndex.at(pblockC->GetHash())));

    // Make I3 the tip now.
    std::shared_ptr<CBlock> pblockI3 = CreateBlockInternal(pwalletMain.get(),
                                                           {},
                                                           mapBlockIndex.at(pblockH3->GetHash()),
                                                           {pblockD3, pblockE3, pblockF3, pblockG3, pblockH3});
    BOOST_CHECK(ProcessNewBlock(pblockI3, nullptr));
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() ==  pblockI3->GetHash()));

    // And rescan the wallet on top of the new chain
    WalletRescanReserver reserver(pwalletMain.get());
    BOOST_CHECK(reserver.reserve());
    pwalletMain->RescanFromTime(0, reserver, true /* update */);

    // #################################################################################
    // ### 7) Now try to use the same coinstake on different chains --> should pass. ###
    // #################################################################################

    // Take I3 coinstake and use it for block I, changing its hash adding a new tx
    std::shared_ptr<CBlock> pblockI = std::make_shared<CBlock>(*pblockI3);
    auto iTx = CreateAndCommitTx(pwalletMain.get(), *dest, 1 * COIN);
    pblockI->vtx.emplace_back(MakeTransactionRef(iTx));
    pblockI->hashMerkleRoot = BlockMerkleRoot(*pblockI);
    assert(SignBlock(*pblockI, *pwalletMain));
    BOOST_CHECK(pblockI3->GetHash() != pblockI->GetHash());
    BOOST_CHECK(ProcessNewBlock(pblockI, nullptr));
}

BOOST_AUTO_TEST_SUITE_END()
