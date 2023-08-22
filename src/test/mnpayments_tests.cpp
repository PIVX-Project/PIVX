<<<<<<< HEAD
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
=======
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"
>>>>>>> 1f345019d (first step)

#include "blockassembler.h"
#include "consensus/merkle.h"
#include "gamemaster-payments.h"
#include "gamemasterman.h"
#include "spork.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "primitives/transaction.h"
#include "utilmoneystr.h"
#include "util/blockstatecatcher.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(gmpayments_tests)

void enableGmSyncAndGMPayments()
{
    // force gmsync complete
    g_tiertwo_sync_state.SetCurrentSyncPhase(GAMEMASTER_SYNC_FINISHED);

    // enable SPORK_13
    int64_t nTime = GetTime() - 10;
    CSporkMessage spork(SPORK_13_ENABLE_SUPERBLOCKS, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS));

    spork = CSporkMessage(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_8_GAMEMASTER_PAYMENT_ENFORCEMENT));
}

static bool CreateGMWinnerPayment(const CTxIn& gmVinVoter, int paymentBlockHeight, const CScript& payeeScript,
                                  const CKey& signerKey, const CPubKey& signerPubKey, CValidationState& state)
{
    CGamemasterPaymentWinner gmWinner(gmVinVoter, paymentBlockHeight);
    gmWinner.AddPayee(payeeScript);
    BOOST_CHECK(gmWinner.Sign(signerKey, signerPubKey.GetID()));
    return gamemasterPayments.ProcessGMWinner(gmWinner, nullptr, state);
}

class GMdata
{
public:
    COutPoint collateralOut;
    CKey gmPrivKey;
    CPubKey gmPubKey;
    CPubKey collateralPubKey;
    CScript gmPayeeScript;

    GMdata(const COutPoint& collateralOut, const CKey& gmPrivKey, const CPubKey& gmPubKey,
           const CPubKey& collateralPubKey, const CScript& gmPayeeScript) :
           collateralOut(collateralOut), gmPrivKey(gmPrivKey), gmPubKey(gmPubKey),
           collateralPubKey(collateralPubKey), gmPayeeScript(gmPayeeScript) {}


};

CGamemaster buildGM(const GMdata& data, const uint256& tipHash, uint64_t tipTime)
{
    CGamemaster gm;
    gm.vin = CTxIn(data.collateralOut);
    gm.pubKeyCollateralAddress = data.gmPubKey;
    gm.pubKeyGamemaster = data.collateralPubKey;
    gm.sigTime = GetTime() - 8000 - 1; // GM_WINNER_MINIMUM_AGE = 8000.
    gm.lastPing = CGamemasterPing(gm.vin, tipHash, tipTime);
    return gm;
}

class FakeGamemaster {
public:
    explicit FakeGamemaster(CGamemaster& gm, const GMdata& data) : gm(gm), data(data) {}
    CGamemaster gm;
    GMdata data;
};

std::vector<FakeGamemaster> buildGMList(const uint256& tipHash, uint64_t tipTime, int size)
{
    std::vector<FakeGamemaster> ret;
    for (int i=0; i < size; i++) {
        CKey gmKey;
        gmKey.MakeNewKey(true);
        const CPubKey& gmPubKey = gmKey.GetPubKey();
        const CScript& gmPayeeScript = GetScriptForDestination(gmPubKey.GetID());
        // Fake collateral out and key for now
        COutPoint gmCollateral(GetRandHash(), 0);
        const CPubKey& collateralPubKey = gmPubKey;

        // Now add the GM
        GMdata gmData(gmCollateral, gmKey, gmPubKey, collateralPubKey, gmPayeeScript);
        CGamemaster gm = buildGM(gmData, tipHash, tipTime);
        BOOST_CHECK(gamemasterman.Add(gm));
        ret.emplace_back(gm, gmData);
    }
    return ret;
}

FakeGamemaster findGMData(std::vector<FakeGamemaster>& gmList, const GamemasterRef& ref)
{
    for (const auto& item : gmList) {
        if (item.data.gmPubKey == ref->pubKeyGamemaster) {
            return item;
        }
    }
    throw std::runtime_error("GM not found");
}

bool findStrError(CValidationState& state, const std::string& str)
{
    return state.GetRejectReason().find(str) != std::string::npos;
}

BOOST_FIXTURE_TEST_CASE(gmwinner_test, TestChain100Setup)
{
    CreateAndProcessBlock({}, coinbaseKey);
    CBlock tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    enableGmSyncAndGMPayments();
    int nextBlockHeight = 103;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_3, nextBlockHeight - 1);

    // GM list.
    std::vector<FakeGamemaster> gmList = buildGMList(tipBlock.GetHash(), tipBlock.GetBlockTime(), 40);
    std::vector<std::pair<int64_t, GamemasterRef>> gmRank = gamemasterman.GetGamemasterRanks(nextBlockHeight - 100);

    // Test gmwinner failure for non-existent GM voter.
    CTxIn dummyVoter;
    CScript dummyPayeeScript;
    CKey dummyKey;
    dummyKey.MakeNewKey(true);
    CValidationState state0;
    BOOST_CHECK(!CreateGMWinnerPayment(dummyVoter, nextBlockHeight, dummyPayeeScript,
                                       dummyKey, dummyKey.GetPubKey(), state0));
    BOOST_CHECK_MESSAGE(findStrError(state0, "Non-existent gmwinner voter"), state0.GetRejectReason());

    // Take the first GM
    auto firstGM = findGMData(gmList, gmRank[0].second);
    CTxIn gmVinVoter(firstGM.gm.vin);
    int paymentBlockHeight = nextBlockHeight;
    CScript payeeScript = firstGM.data.gmPayeeScript;
    CGamemaster* pFirstGM = gamemasterman.Find(firstGM.gm.vin.prevout);
    pFirstGM->sigTime += 8000 + 1; // GM_WINNER_MINIMUM_AGE = 8000.
    // Voter GM1, fail because the sigTime - GetAdjustedTime() is not greater than GM_WINNER_MINIMUM_AGE.
    CValidationState state1;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, payeeScript,
                                       firstGM.data.gmPrivKey, firstGM.data.gmPubKey, state1));
    // future: add specific error cause
    BOOST_CHECK_MESSAGE(findStrError(state1, "Gamemaster not in the top"), state1.GetRejectReason());

    // Voter GM2, fail because GM2 doesn't match with the signing keys.
    auto secondGm = findGMData(gmList, gmRank[1].second);
    CGamemaster* pSecondGM = gamemasterman.Find(secondGm.gm.vin.prevout);
    gmVinVoter = CTxIn(pSecondGM->vin);
    payeeScript = secondGm.data.gmPayeeScript;
    CValidationState state2;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, payeeScript,
                                       firstGM.data.gmPrivKey, firstGM.data.gmPubKey, state2));
    BOOST_CHECK_MESSAGE(findStrError(state2, "invalid voter gmwinner signature"), state2.GetRejectReason());

    // Voter GM2, fail because gmwinner height is too far in the future.
    gmVinVoter = CTxIn(pSecondGM->vin);
    CValidationState state2_5;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight + 20, payeeScript,
                                       secondGm.data.gmPrivKey, secondGm.data.gmPubKey, state2_5));
    BOOST_CHECK_MESSAGE(findStrError(state2_5, "block height out of range"), state2_5.GetRejectReason());


    // Voter GM2, fail because GM2 is not enabled
    pSecondGM->SetSpent();
    BOOST_CHECK(!pSecondGM->IsEnabled());
    CValidationState state3;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, payeeScript,
                                       secondGm.data.gmPrivKey, secondGm.data.gmPubKey, state3));
    // future: could add specific error cause.
    BOOST_CHECK_MESSAGE(findStrError(state3, "Gamemaster not in the top"), state3.GetRejectReason());

    // Voter GM3, fail because the payeeScript is not a P2PKH
    auto thirdGm = findGMData(gmList, gmRank[2].second);
    CGamemaster* pThirdGM = gamemasterman.Find(thirdGm.gm.vin.prevout);
    gmVinVoter = CTxIn(pThirdGM->vin);
    CScript scriptDummy = CScript() << OP_TRUE;
    CValidationState state4;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, scriptDummy,
                                       thirdGm.data.gmPrivKey, thirdGm.data.gmPubKey, state4));
    BOOST_CHECK_MESSAGE(findStrError(state4, "payee must be a P2PKH"), state4.GetRejectReason());

    // Voter GM15 pays to GM3, fail because the voter is not in the top ten.
    auto voterPos15 = findGMData(gmList, gmRank[14].second);
    CGamemaster* p15dGM = gamemasterman.Find(voterPos15.gm.vin.prevout);
    gmVinVoter = CTxIn(p15dGM->vin);
    payeeScript = thirdGm.data.gmPayeeScript;
    CValidationState state6;
    BOOST_CHECK(!CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, payeeScript,
                                       voterPos15.data.gmPrivKey, voterPos15.data.gmPubKey, state6));
    BOOST_CHECK_MESSAGE(findStrError(state6, "Gamemaster not in the top"), state6.GetRejectReason());

    // Voter GM3, passes
    gmVinVoter = CTxIn(pThirdGM->vin);
    CValidationState state7;
    BOOST_CHECK(CreateGMWinnerPayment(gmVinVoter, paymentBlockHeight, payeeScript,
                                      thirdGm.data.gmPrivKey, thirdGm.data.gmPubKey, state7));
    BOOST_CHECK_MESSAGE(state7.IsValid(), state7.GetRejectReason());

    // Create block and check that is being paid properly.
    tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == payeeScript, "error: block not paying to proper GM");
    nextBlockHeight++;

    // Now let's push two valid winner payments and make every GM in the top ten vote for them (having more votes in gmwinnerA than in gmwinnerB).
    gmRank = gamemasterman.GetGamemasterRanks(nextBlockHeight - 100);
    CScript firstRankedPayee = GetScriptForDestination(gmRank[0].second->pubKeyCollateralAddress.GetID());
    CScript secondRankedPayee = GetScriptForDestination(gmRank[1].second->pubKeyCollateralAddress.GetID());

    // Let's vote with the first 6 nodes for GM ranked 1
    // And with the last 4 nodes for GM ranked 2
    payeeScript = firstRankedPayee;
    for (int i=0; i<10; i++) {
        if (i > 5) {
            payeeScript = secondRankedPayee;
        }
        auto voterGm = findGMData(gmList, gmRank[i].second);
        CGamemaster* pVoterGM = gamemasterman.Find(voterGm.gm.vin.prevout);
        gmVinVoter = CTxIn(pVoterGM->vin);
        CValidationState stateInternal;
        BOOST_CHECK(CreateGMWinnerPayment(gmVinVoter, nextBlockHeight, payeeScript,
                                                             voterGm.data.gmPrivKey, voterGm.data.gmPubKey, stateInternal));
        BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
    }

    // Check the votes count for each gmwinner.
    CGamemasterBlockPayees blockPayees = gamemasterPayments.mapGamemasterBlocks.at(nextBlockHeight);
    BOOST_CHECK_MESSAGE(blockPayees.HasPayeeWithVotes(firstRankedPayee, 6), "first ranked payee with no enough votes");
    BOOST_CHECK_MESSAGE(blockPayees.HasPayeeWithVotes(secondRankedPayee, 4), "second ranked payee with no enough votes");

    // let's try to create a bad block paying to the second most voted GM.
    CBlock badBlock = CreateBlock({}, coinbaseKey);
    CMutableTransaction coinbase(*badBlock.vtx[0]);
    coinbase.vout[coinbase.vout.size() - 1].scriptPubKey = secondRankedPayee;
    badBlock.vtx[0] = MakeTransactionRef(coinbase);
    badBlock.hashMerkleRoot = BlockMerkleRoot(badBlock);
    {
        auto pBadBlock = std::make_shared<CBlock>(badBlock);
        SolveBlock(pBadBlock, nextBlockHeight);
        BlockStateCatcher sc(pBadBlock->GetHash());
        sc.registerEvent();
        ProcessNewBlock(pBadBlock, nullptr);
        BOOST_CHECK(sc.found && !sc.state.IsValid());
        BOOST_CHECK_EQUAL(sc.state.GetRejectReason(), "bad-cb-payee");
    }
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Tip()->GetBlockHash();) != badBlock.GetHash());


    // And let's verify that the most voted one is the one being paid.
    tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == firstRankedPayee, "error: block not paying to first ranked GM");
    nextBlockHeight++;

    //
    // Generate 125 blocks paying to different GMs to load the payments cache.
    for (int i = 0; i < 125; i++) {
        gmRank = gamemasterman.GetGamemasterRanks(nextBlockHeight - 100);
        payeeScript = GetScriptForDestination(gmRank[0].second->pubKeyCollateralAddress.GetID());
        for (int j=0; j<7; j++) { // votes
            auto voterGm = findGMData(gmList, gmRank[j].second);
            CGamemaster* pVoterGM = gamemasterman.Find(voterGm.gm.vin.prevout);
            gmVinVoter = CTxIn(pVoterGM->vin);
            CValidationState stateInternal;
            BOOST_CHECK(CreateGMWinnerPayment(gmVinVoter, nextBlockHeight, payeeScript,
                                              voterGm.data.gmPrivKey, voterGm.data.gmPubKey, stateInternal));
            BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
        }
        // Create block and check that is being paid properly.
        tipBlock = CreateAndProcessBlock({}, coinbaseKey);
        BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == payeeScript, "error: block not paying to proper GM");
        nextBlockHeight++;
    }
    // Check chain height.
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Height();), nextBlockHeight - 1);

    // Let's now verify what happen if a previously paid GM goes offline but still have scheduled a payment in the future.
    // The current system allows it (up to a certain point) as payments are scheduled ahead of time and a GM can go down in the
    // [proposedWinnerHeightTime < currentHeight < currentHeight + 20] window.

    // 1) Schedule payment and vote for it with the first 6 GMs.
    gmRank = gamemasterman.GetGamemasterRanks(nextBlockHeight - 100);
    GamemasterRef gmToPay = gmRank[0].second;
    payeeScript = GetScriptForDestination(gmToPay->pubKeyCollateralAddress.GetID());
    for (int i=0; i<6; i++) {
        auto voterGm = findGMData(gmList, gmRank[i].second);
        CGamemaster* pVoterGM = gamemasterman.Find(voterGm.gm.vin.prevout);
        gmVinVoter = CTxIn(pVoterGM->vin);
        CValidationState stateInternal;
        BOOST_CHECK(CreateGMWinnerPayment(gmVinVoter, nextBlockHeight, payeeScript,
                                          voterGm.data.gmPrivKey, voterGm.data.gmPubKey, stateInternal));
        BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
    }

    // 2) Remove payee GM from the GM list and try to emit a vote from GM7 to the same payee.
    // it should still be accepted because the GM was scheduled when it was online.
    gamemasterman.Remove(gmToPay->vin.prevout);
    BOOST_CHECK_MESSAGE(!gamemasterman.Find(gmToPay->vin.prevout), "error: removed GM is still available");

    // Now emit the vote from GM7
    auto voterGm = findGMData(gmList, gmRank[7].second);
    CGamemaster* pVoterGM = gamemasterman.Find(voterGm.gm.vin.prevout);
    gmVinVoter = CTxIn(pVoterGM->vin);
    CValidationState stateInternal;
    BOOST_CHECK(CreateGMWinnerPayment(gmVinVoter, nextBlockHeight, payeeScript,
                                      voterGm.data.gmPrivKey, voterGm.data.gmPubKey, stateInternal));
    BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
}

BOOST_AUTO_TEST_SUITE_END()