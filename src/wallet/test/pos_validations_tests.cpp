// Copyright (c) 2020-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "optional.h"
#include "primitives/transaction.h"
#include "sapling/address.h"
#include "sapling/zip32.h"
#include "sync.h"
#include "validation.h"
#include "wallet/test/pos_test_fixture.h"

#include "blockassembler.h"
#include "blocksignature.h"
#include "coincontrol.h"
#include "consensus/merkle.h"
#include "primitives/block.h"
#include "sapling/sapling_operation.h"
#include "script/sign.h"
#include "test/util/blocksutil.h"
#include "util/blockstatecatcher.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>
#include <memory>
#include <string>
#include <vector>

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
    std::vector<CStakeableOutput> availableUTXOs;
    BOOST_CHECK(pwalletMain->StakeableUTXOs(&availableUTXOs));
    std::vector<std::unique_ptr<CStakeableInterface>> availableCoins;
    for (auto& utxo : availableUTXOs) {
        availableCoins.push_back(std::make_unique<CStakeableOutput>(utxo));
    }
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
        Params(), false)
                                                         .CreateNewBlock(CScript(),
                                                             pwalletMain.get(),
                                                             true,
                                                             availableCoins,
                                                             true);
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
    BOOST_CHECK(pblock->IsProofOfStake());

    // Add a second input to a coinstake
    CMutableTransaction mtx(*pblock->vtx[1]);
    const CStakeableOutput& in2 = availableUTXOs.back();
    availableUTXOs.pop_back();
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

std::shared_ptr<CBlock> CreateBlockInternal(CWallet* pwalletMain, const std::vector<CMutableTransaction>& txns = {}, CBlockIndex* customPrevBlock = nullptr, std::initializer_list<std::shared_ptr<CBlock>> forkchain = {}, bool fNoMempoolTx = true, bool isShieldStake = false)
{
    std::vector<std::unique_ptr<CStakeableInterface>> availableCoins;
    if (!isShieldStake) {
        std::vector<CStakeableOutput> availableUTXOs;
        BOOST_CHECK(pwalletMain->StakeableUTXOs(&availableUTXOs));

        // Remove any utxo which is not deeper than 120 blocks (for the same reasoning
        // used when selecting tx inputs in CreateAndCommitTx)
        // Also, as the wallet is not prepared to follow several chains at the same time,
        // need to manually remove from the stakeable utxo set every already used
        // coinstake inputs on the previous blocks of the parallel chain so they
        // are not used again.
        for (auto it = availableUTXOs.begin(); it != availableUTXOs.end();) {
            if (it->nDepth <= 120 || IsSpentOnFork(*it, forkchain)) {
                it = availableUTXOs.erase(it);
            } else {
                it++;
            }
        }

        for (auto& utxo : availableUTXOs) {
            availableCoins.push_back(std::make_unique<CStakeableOutput>(utxo));
        }
    } else {
        std::vector<CStakeableShieldNote> availableNotes;
        BOOST_CHECK(pwalletMain->StakeableNotes(&availableNotes));
        for (auto& note : availableNotes) {
            availableCoins.push_back(std::make_unique<CStakeableShieldNote>(note));
        }
    }
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
        Params(), false)
                                                         .CreateNewBlock(CScript(),
                                                             pwalletMain,
                                                             true,
                                                             availableCoins,
                                                             fNoMempoolTx,
                                                             false,
                                                             customPrevBlock,
                                                             false);
    BOOST_ASSERT(pblocktemplate);
    auto pblock = std::make_shared<CBlock>(pblocktemplate->block);
    if (!txns.empty()) {
        if (isShieldStake) BOOST_CHECK(false);
        for (const auto& tx : txns) {
            pblock->vtx.emplace_back(MakeTransactionRef(tx));
        }
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        const int nHeight = (customPrevBlock != nullptr ? customPrevBlock->nHeight + 1 : WITH_LOCK(cs_main, return chainActive.Height()) + 1);
        pblock->hashFinalSaplingRoot = CalculateSaplingTreeRoot(&*pblock, nHeight, Params());
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

// From now on SHIELD STAKE TESTS
static void ActivateShieldStaking(CWallet* pwalletMain)
{
    while (WITH_LOCK(cs_main, return chainActive.Tip()->nHeight) < 600) {
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain);
        ProcessNewBlock(pblock, nullptr);
    }
}

static void UpdateAndProcessShieldStakeBlock(std::shared_ptr<CBlock> pblock, CWallet* pwalletMain, std::string processError, int expBlock)
{
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    pblock->hashFinalSaplingRoot = CalculateSaplingTreeRoot(&*pblock, WITH_LOCK(cs_main, return chainActive.Height()) + 1, Params());
    if (pblock->IsProofOfShieldStake()) BOOST_CHECK(SignBlock(*pblock, *pwalletMain, pblock->vtx[1]->shieldStakeRandomness, pblock->vtx[1]->shieldStakePrivKey));
    ProcessBlockAndCheckRejectionReason(pblock, processError, expBlock);
}

// Create a coinshieldstake with an eventual addition of unwanted pieces
// Entries of shieldNotes are the notes you are going to spend inside the coinshieldstake
static bool CreateFakeShieldReward(CWallet* pwalletMain, const std::vector<CStakeableShieldNote>& shieldNotes, CMutableTransaction& txNew, int deltaReward, bool splitOutput, bool addMultiEmptyOutput)
{
    int nHeight = WITH_LOCK(cs_main, return chainActive.Tip()->nHeight);
    CAmount nMasternodePayment = GetMasternodePayment(nHeight);
    TransactionBuilder txBuilder(Params().GetConsensus(), pwalletMain);
    txBuilder.SetFee(0);
    txBuilder.AddStakeInput();
    if (addMultiEmptyOutput) {
        txBuilder.AddStakeInput();
    }
    CAmount val = GetBlockValue(nHeight) - nMasternodePayment + deltaReward * COIN;
    for (const CStakeableShieldNote& note : shieldNotes) {
        val += note.note.value();
    }
    val /= (splitOutput + 1);
    for (int i = 0; i < (splitOutput + 1); i++) {
        txBuilder.AddSaplingOutput(pwalletMain->GetSaplingScriptPubKeyMan()->getCommonOVK(), pwalletMain->GenerateNewSaplingZKey(), val);
    }

    std::vector<libzcash::SaplingExtendedSpendingKey> sk;
    for (const CStakeableShieldNote& note : shieldNotes) {
        libzcash::SaplingExtendedSpendingKey t;
        if (!pwalletMain->GetSaplingExtendedSpendingKey(note.address, t)) {
            return false;
        }
        sk.push_back(t);
    }

    uint256 anchor;
    std::vector<Optional<SaplingWitness>> witnesses;
    std::vector<SaplingOutPoint> noteop;
    for (const CStakeableShieldNote& note : shieldNotes) {
        noteop.emplace_back(note.op);
    }

    pwalletMain->GetSaplingScriptPubKeyMan()->GetSaplingNoteWitnesses(noteop, witnesses, anchor);
    int i = 0;
    for (const CStakeableShieldNote& note : shieldNotes) {
        txBuilder.AddSaplingSpend(sk[i].expsk, note.note, anchor, witnesses[i].get());
        i++;
    }

    const auto& txTrial = txBuilder.Build().GetTx();
    if (txTrial) {
        txNew = CMutableTransaction(*txTrial);
        txNew.shieldStakePrivKey = sk[0].expsk.ask;
        txNew.shieldStakeRandomness = txBuilder.GetShieldStakeRandomness();
        return true;
    } else {
        return false;
    }
}

// Create a sapling operation that can build a shield tx
static SaplingOperation CreateOperationAndBuildTx(std::unique_ptr<CWallet>& pwallet,
    CAmount amount,
    bool selectTransparentCoins)
{
    // Create the operation
    libzcash::SaplingPaymentAddress pa = pwallet->GenerateNewSaplingZKey("s1");
    std::vector<SendManyRecipient> recipients;
    recipients.emplace_back(pa, amount, "", false);
    SaplingOperation operation(Params().GetConsensus(), pwallet.get());
    operation.setMinDepth(1);
    auto operationResult = operation.setRecipients(recipients)
                               ->setSelectTransparentCoins(selectTransparentCoins)
                               ->setSelectShieldedCoins(!selectTransparentCoins)
                               ->build();
    BOOST_ASSERT_MSG(operationResult, operationResult.getError().c_str());

    CValidationState state;
    BOOST_ASSERT_MSG(
        CheckTransaction(operation.getFinalTx(), state, true),
        "Invalid Sapling transaction");
    return operation;
}

// The aim of this test is verifying some basic rules regarding the coinshieldstake,
// double spend and non malleability of the shield staked block
BOOST_FIXTURE_TEST_CASE(coinshieldstake_tests, TestPoSChainSetup)
{
    // Verify that we are at block 250 and then activate shield Staking
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->nHeight), 250);
    SyncWithValidationInterfaceQueue();
    ActivateShieldStaking(pwalletMain.get());

    // Create two sapling notes with 10k PIVs
    {
        CReserveKey reservekey(&*pwalletMain);
        for (int i = 0; i < 2; i++) {
            SaplingOperation operation = CreateOperationAndBuildTx(pwalletMain, 10000 * COIN, true);
            pwalletMain->CommitTransaction(operation.getFinalTxRef(), reservekey, nullptr);
        }
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, false);
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));

        // Sanity check on the block created
        BOOST_CHECK(pblock->vtx.size() == 4);
        BOOST_CHECK(pblock->vtx[2]->sapData->vShieldedOutput.size() == 1 && pblock->vtx[3]->sapData->vShieldedOutput.size() == 1);
    }

    // Create 20 more blocks, in such a way that the sapling notes will be stakeable
    for (int i = 0; i < 20; i++) {
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, true);
        ProcessNewBlock(pblock, nullptr);
    }

    // Create a shield stake block
    std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, false, true);
    BOOST_CHECK(pblock->IsProofOfShieldStake());

    // And let's begin with tests:
    // 1) ShieldStake blocks are not malleable, for example let's try to add a new tx
    {
        std::shared_ptr<CBlock> pblockA = std::make_shared<CBlock>(*pblock);
        auto cTx = CreateAndCommitTx(pwalletMain.get(), *pwalletMain->getNewAddress("").getObjResult(), 249 * COIN);
        pblockA->vtx.emplace_back(MakeTransactionRef(cTx));
        pblockA->hashMerkleRoot = BlockMerkleRoot(*pblockA);
        pblockA->hashFinalSaplingRoot = CalculateSaplingTreeRoot(&*pblockA, WITH_LOCK(cs_main, return chainActive.Height()) + 1, Params());
        ProcessBlockAndCheckRejectionReason(pblockA, "bad-PoS-sig", 621);
    }

    // 2) The Note used to ShieldStake cannot be spent two times in the same block:
    {
        std::shared_ptr<CBlock> pblockB = std::make_shared<CBlock>(*pblock);
        uint256 shieldStakeNullifier = pblock.get()->vtx[1]->sapData->vShieldedSpend[0].nullifier;

        // Build a random shield tx that spends the shield stake note (we are spending both to be sure).
        SaplingOperation operation = CreateOperationAndBuildTx(pwalletMain, 11000 * COIN, false);
        // Sanity check on the notes that was spent
        auto vecShieldSpend = operation.getFinalTx().sapData->vShieldedSpend;
        BOOST_CHECK(vecShieldSpend[0].nullifier == shieldStakeNullifier || vecShieldSpend[1].nullifier == shieldStakeNullifier);
        BOOST_CHECK(operation.getFinalTx().sapData->vShieldedSpend.size() == 2);

        // Update the block, resign and try to process
        pblockB->vtx.emplace_back(operation.getFinalTxRef());
        UpdateAndProcessShieldStakeBlock(pblockB, &*pwalletMain, "bad-txns-sapling-requirements-not-met", 621);
    }
    // 3) Let's try to change the structure of the CoinShieldStake tx:
    {
        std::shared_ptr<CBlock> pblockC = std::make_shared<CBlock>(*pblock);
        uint256 shieldStakeNullifier = pblock.get()->vtx[1]->sapData->vShieldedSpend[0].nullifier;
        std::vector<CStakeableShieldNote> stakeableNotes = {};
        BOOST_CHECK(pwalletMain->StakeableNotes(&stakeableNotes));
        // Usual sanity check
        BOOST_CHECK(stakeableNotes.size() == 2);
        int shieldNoteIndex = stakeableNotes[0].nullifier == shieldStakeNullifier ? 0 : 1;
        int otherNoteIndex = (1 + shieldNoteIndex) % 2;

        // 3.1) Staker is trying to get paid a different amount from the expected
        CMutableTransaction mtx;
        BOOST_CHECK(CreateFakeShieldReward(&*pwalletMain, {stakeableNotes[shieldNoteIndex]}, mtx, 100000, false, false));
        pblockC->vtx[1] = MakeTransactionRef(mtx);
        UpdateAndProcessShieldStakeBlock(pblockC, &*pwalletMain, "bad-blk-amount", 621);

        // 3.2) Staker is trying to add more than one empty vout
        BOOST_CHECK(CreateFakeShieldReward(&*pwalletMain, {stakeableNotes[shieldNoteIndex]}, mtx, 0, false, true));
        pblockC->vtx[1] = MakeTransactionRef(mtx);
        // Why this processError? Well the mtx is not coinstake since it has saplingdata and the mtx is not coinshieldstake since the vout has length different than 1,
        // therefore the block is not proof of stake => invalid PoW
        UpdateAndProcessShieldStakeBlock(pblockC, &*pwalletMain, "PoW-ended", 621);

        // 3.3) Staker is trying to add another shield input
        BOOST_CHECK(CreateFakeShieldReward(&*pwalletMain, {stakeableNotes[shieldNoteIndex], stakeableNotes[otherNoteIndex]}, mtx, 0, false, false));
        pblockC->vtx[1] = MakeTransactionRef(mtx);
        UpdateAndProcessShieldStakeBlock(pblockC, &*pwalletMain, "bad-scs-multi-inputs", 621);

        // 3.4) TODO: add an upper bound on the maximum number of shield outputs (or a malicious node could create huge blocks without paying fees)??
    }
    // 4) TODO: test what happens is the proof is faked

    // 5) Last but not least, the original block is processed without any errors.
    BOOST_CHECK(ProcessNewBlock(pblock, nullptr));
}

// The aim of this test is verifying that shield stake rewards can be spent only as intended.
BOOST_FIXTURE_TEST_CASE(shieldstake_fork_tests, TestPoSChainSetup)
{
    /*
    Consider the following chain diagram:
    A -- B -- C -- D -- E -- F
              \
                -- D1 -- E1 --F1 -- G1

    I will verify that a shield stake reward created in D can be spent in F but not in the fork block E1
    */
    // Verify that we are at block 250 and then activate shield Staking
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Tip()->nHeight), 250);
    SyncWithValidationInterfaceQueue();
    ActivateShieldStaking(pwalletMain.get());
    // Create a sapling notes of 10k PIVs
    {
        CReserveKey reservekey(&*pwalletMain);
        SaplingOperation operation = CreateOperationAndBuildTx(pwalletMain, 10000 * COIN, true);
        pwalletMain->CommitTransaction(operation.getFinalTxRef(), reservekey, nullptr);

        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, false);
        BOOST_CHECK(ProcessNewBlock(pblock, nullptr));

        // Sanity check on the block created
        BOOST_CHECK(pblock->vtx.size() == 3);
        BOOST_CHECK(pblock->vtx[2]->sapData->vShieldedOutput.size() == 1);
    }

    // Create 20 more blocks, in such a way that the sapling notes will be stakeable, the 20-th is block C
    for (int i = 0; i < 19; i++) {
        std::shared_ptr<CBlock> pblock = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, true);
        ProcessNewBlock(pblock, nullptr);
    }
    std::shared_ptr<CBlock> pblockC = CreateBlockInternal(pwalletMain.get());
    BOOST_CHECK(ProcessNewBlock(pblockC, nullptr));

    // Create a shielded pos block D
    std::shared_ptr<CBlock> pblockD = CreateBlockInternal(pwalletMain.get(), {}, nullptr, {}, true, true);

    // Create D1 forked block that connects a new tx
    std::shared_ptr<CBlock> pblockD1 = CreateBlockInternal(pwalletMain.get());

    // Process blocks D and D1
    ProcessNewBlock(pblockD, nullptr);
    ProcessNewBlock(pblockD1, nullptr);
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() == pblockD->GetHash()));

    // Create block E
    std::shared_ptr<CBlock> pblockE = CreateBlockInternal(pwalletMain.get(), {}, {});
    BOOST_CHECK(ProcessNewBlock(pblockE, nullptr));

    // Verify that we indeed have the shield stake reward:
    std::vector<CStakeableShieldNote> notes = {};
    BOOST_CHECK(pwalletMain->GetSaplingScriptPubKeyMan()->GetStakeableNotes(&notes, 1));
    BOOST_CHECK(notes.size() == 1);
    BOOST_CHECK(notes[0].note.value() == 10004 * COIN);
    uint256 rewardNullifier = notes[0].nullifier;

    // Build and commit a tx that spends the reward and check that it indeed spends it
    auto operation = CreateOperationAndBuildTx(pwalletMain, 100 * COIN, false);
    auto txRef = operation.getFinalTx();
    BOOST_CHECK(txRef.sapData->vShieldedSpend.size() == 1);
    BOOST_CHECK(txRef.sapData->vShieldedSpend[0].nullifier == rewardNullifier);
    std::string txHash;
    operation.send(txHash);

    // Create the forked block E1 that spends the shield stake reward
    // There is a little catch here: the validation checks for this kind of invalid block (in this case invalid anchor since the note does not exist on the forked chain)
    // is done only on ConnectBlock which is called only when we are connecting the new block to the current chaintip
    // now, since the fork is not the current active chain ConnectBlock is not called and the block seems to be valid.
    std::shared_ptr<CBlock> pblockE1 = CreateBlockInternal(pwalletMain.get(), {txRef}, mapBlockIndex.at(pblockD1->GetHash()), {pblockD1});
    BOOST_CHECK(pblockE1->vtx[2]->sapData->vShieldedSpend.size() == 1);
    BOOST_CHECK(pblockE1->vtx[2]->sapData->vShieldedSpend[0].nullifier == rewardNullifier);
    BOOST_CHECK(ProcessNewBlock(pblockE1, nullptr));

    // So how to see that the forked chain is indeed invalid?
    // We can keep adding blocks to the forked chain until it becomes the active chain!
    // In that moment connectblock will be called and the forked chain will become invalid.
    std::shared_ptr<CBlock> pblockF1 = CreateBlockInternal(pwalletMain.get(), {}, mapBlockIndex.at(pblockE1->GetHash()), {pblockD1, pblockE1});
    BOOST_CHECK(ProcessNewBlock(pblockF1, nullptr));

    // finally the wallet will try to activate the forkedchain and will figure out that we have a bad previous block
    std::shared_ptr<CBlock> pblockG1 = CreateBlockInternal(pwalletMain.get(), {}, mapBlockIndex.at(pblockF1->GetHash()), {pblockD1, pblockE1, pblockF1});
    ProcessBlockAndCheckRejectionReason(pblockG1, "bad-prevblk", 623);
    // Side note: Of course what I just showed doesn't depend on shield staking and will happen for any non-valid shield transaction sent to a forked chain,
    // TODO: change the validation in such a way that the anchor and duplicated nullifiers are checked not only when the block is connected?

    // Finally let's see that the shield reward can be spent on the main chain:
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash() == pblockE->GetHash()));
    std::shared_ptr<CBlock> pblockF = CreateBlockInternal(pwalletMain.get(), {txRef});
    BOOST_CHECK(ProcessNewBlock(pblockF, nullptr));
}

BOOST_AUTO_TEST_SUITE_END()
