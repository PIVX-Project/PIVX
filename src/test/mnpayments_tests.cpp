// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include "blockassembler.h"
#include "consensus/merkle.h"
#include "masternode-payments.h"
#include "masternodeman.h"
#include "spork.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "primitives/transaction.h"
#include "utilmoneystr.h"
#include "util/blockstatecatcher.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(mnpayments_tests)

void enableMnSyncAndMNPayments()
{
    // force mnsync complete
    g_tiertwo_sync_state.SetCurrentSyncPhase(MASTERNODE_SYNC_FINISHED);

    // enable SPORK_13
    int64_t nTime = GetTime() - 10;
    CSporkMessage spork(SPORK_13_ENABLE_SUPERBLOCKS, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS));

    spork = CSporkMessage(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT, nTime + 1, nTime);
    sporkManager.AddOrUpdateSporkMessage(spork);
    BOOST_CHECK(sporkManager.IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT));
}

static bool CreateMNWinnerPayment(const CTxIn& mnVinVoter, int paymentBlockHeight, const CScript& payeeScript,
                                  const CKey& signerKey, const CPubKey& signerPubKey, CValidationState& state)
{
    CMasternodePaymentWinner mnWinner(mnVinVoter, paymentBlockHeight);
    mnWinner.AddPayee(payeeScript);
    BOOST_CHECK(mnWinner.Sign(signerKey, signerPubKey.GetID()));
    return masternodePayments.ProcessMNWinner(mnWinner, nullptr, state);
}

class MNdata
{
public:
    COutPoint collateralOut;
    CKey mnPrivKey;
    CPubKey mnPubKey;
    CPubKey collateralPubKey;
    CScript mnPayeeScript;

    MNdata(const COutPoint& collateralOut, const CKey& mnPrivKey, const CPubKey& mnPubKey,
           const CPubKey& collateralPubKey, const CScript& mnPayeeScript) :
           collateralOut(collateralOut), mnPrivKey(mnPrivKey), mnPubKey(mnPubKey),
           collateralPubKey(collateralPubKey), mnPayeeScript(mnPayeeScript) {}


};

CMasternode buildMN(const MNdata& data, const uint256& tipHash, uint64_t tipTime)
{
    CMasternode mn;
    mn.vin = CTxIn(data.collateralOut);
    mn.pubKeyCollateralAddress = data.mnPubKey;
    mn.pubKeyMasternode = data.collateralPubKey;
    mn.sigTime = GetTime() - 8000 - 1; // MN_WINNER_MINIMUM_AGE = 8000.
    mn.lastPing = CMasternodePing(mn.vin, tipHash, tipTime);
    return mn;
}

class FakeMasternode {
public:
    explicit FakeMasternode(CMasternode& mn, const MNdata& data) : mn(mn), data(data) {}
    CMasternode mn;
    MNdata data;
};

std::vector<FakeMasternode> buildMNList(const uint256& tipHash, uint64_t tipTime, int size)
{
    std::vector<FakeMasternode> ret;
    for (int i=0; i < size; i++) {
        CKey mnKey;
        mnKey.MakeNewKey(true);
        const CPubKey& mnPubKey = mnKey.GetPubKey();
        const CScript& mnPayeeScript = GetScriptForDestination(mnPubKey.GetID());
        // Fake collateral out and key for now
        COutPoint mnCollateral(GetRandHash(), 0);
        const CPubKey& collateralPubKey = mnPubKey;

        // Now add the MN
        MNdata mnData(mnCollateral, mnKey, mnPubKey, collateralPubKey, mnPayeeScript);
        CMasternode mn = buildMN(mnData, tipHash, tipTime);
        BOOST_CHECK(mnodeman.Add(mn));
        ret.emplace_back(mn, mnData);
    }
    return ret;
}

FakeMasternode findMNData(std::vector<FakeMasternode>& mnList, const MasternodeRef& ref)
{
    for (const auto& item : mnList) {
        if (item.data.mnPubKey == ref->pubKeyMasternode) {
            return item;
        }
    }
    throw std::runtime_error("MN not found");
}

bool findStrError(CValidationState& state, const std::string& str)
{
    return state.GetRejectReason().find(str) != std::string::npos;
}

BOOST_FIXTURE_TEST_CASE(mnwinner_test, TestChain100Setup)
{
    CreateAndProcessBlock({}, coinbaseKey);
    CBlock tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    enableMnSyncAndMNPayments();
    int nextBlockHeight = 103;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V5_3, nextBlockHeight - 1);

    // MN list.
    std::vector<FakeMasternode> mnList = buildMNList(tipBlock.GetHash(), tipBlock.GetBlockTime(), 40);
    std::vector<std::pair<int64_t, MasternodeRef>> mnRank = mnodeman.GetMasternodeRanks(nextBlockHeight - 100);

    // Test mnwinner failure for non-existent MN voter.
    CTxIn dummyVoter;
    CScript dummyPayeeScript;
    CKey dummyKey;
    dummyKey.MakeNewKey(true);
    CValidationState state0;
    BOOST_CHECK(!CreateMNWinnerPayment(dummyVoter, nextBlockHeight, dummyPayeeScript,
                                       dummyKey, dummyKey.GetPubKey(), state0));
    BOOST_CHECK_MESSAGE(findStrError(state0, "Non-existent mnwinner voter"), state0.GetRejectReason());

    // Take the first MN
    auto firstMN = findMNData(mnList, mnRank[0].second);
    CTxIn mnVinVoter(firstMN.mn.vin);
    int paymentBlockHeight = nextBlockHeight;
    CScript payeeScript = firstMN.data.mnPayeeScript;
    CMasternode* pFirstMN = mnodeman.Find(firstMN.mn.vin.prevout);
    pFirstMN->sigTime += 8000 + 1; // MN_WINNER_MINIMUM_AGE = 8000.
    // Voter MN1, fail because the sigTime - GetAdjustedTime() is not greater than MN_WINNER_MINIMUM_AGE.
    CValidationState state1;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       firstMN.data.mnPrivKey, firstMN.data.mnPubKey, state1));
    // future: add specific error cause
    BOOST_CHECK_MESSAGE(findStrError(state1, "Masternode not in the top"), state1.GetRejectReason());

    // Voter MN2, fail because MN2 doesn't match with the signing keys.
    auto secondMn = findMNData(mnList, mnRank[1].second);
    CMasternode* pSecondMN = mnodeman.Find(secondMn.mn.vin.prevout);
    mnVinVoter = CTxIn(pSecondMN->vin);
    payeeScript = secondMn.data.mnPayeeScript;
    CValidationState state2;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       firstMN.data.mnPrivKey, firstMN.data.mnPubKey, state2));
    BOOST_CHECK_MESSAGE(findStrError(state2, "invalid voter mnwinner signature"), state2.GetRejectReason());

    // Voter MN2, fail because mnwinner height is too far in the future.
    mnVinVoter = CTxIn(pSecondMN->vin);
    CValidationState state2_5;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight + 20, payeeScript,
                                       secondMn.data.mnPrivKey, secondMn.data.mnPubKey, state2_5));
    BOOST_CHECK_MESSAGE(findStrError(state2_5, "block height out of range"), state2_5.GetRejectReason());


    // Voter MN2, fail because MN2 is not enabled
    pSecondMN->SetSpent();
    BOOST_CHECK(!pSecondMN->IsEnabled());
    CValidationState state3;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       secondMn.data.mnPrivKey, secondMn.data.mnPubKey, state3));
    // future: could add specific error cause.
    BOOST_CHECK_MESSAGE(findStrError(state3, "Masternode not in the top"), state3.GetRejectReason());

    // Voter MN3, fail because the payeeScript is not a P2PKH
    auto thirdMn = findMNData(mnList, mnRank[2].second);
    CMasternode* pThirdMN = mnodeman.Find(thirdMn.mn.vin.prevout);
    mnVinVoter = CTxIn(pThirdMN->vin);
    CScript scriptDummy = CScript() << OP_TRUE;
    CValidationState state4;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, scriptDummy,
                                       thirdMn.data.mnPrivKey, thirdMn.data.mnPubKey, state4));
    BOOST_CHECK_MESSAGE(findStrError(state4, "payee must be a P2PKH"), state4.GetRejectReason());

    // Voter MN15 pays to MN3, fail because the voter is not in the top ten.
    auto voterPos15 = findMNData(mnList, mnRank[14].second);
    CMasternode* p15dMN = mnodeman.Find(voterPos15.mn.vin.prevout);
    mnVinVoter = CTxIn(p15dMN->vin);
    payeeScript = thirdMn.data.mnPayeeScript;
    CValidationState state6;
    BOOST_CHECK(!CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                       voterPos15.data.mnPrivKey, voterPos15.data.mnPubKey, state6));
    BOOST_CHECK_MESSAGE(findStrError(state6, "Masternode not in the top"), state6.GetRejectReason());

    // Voter MN3, passes
    mnVinVoter = CTxIn(pThirdMN->vin);
    CValidationState state7;
    BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, paymentBlockHeight, payeeScript,
                                      thirdMn.data.mnPrivKey, thirdMn.data.mnPubKey, state7));
    BOOST_CHECK_MESSAGE(state7.IsValid(), state7.GetRejectReason());

    // Create block and check that is being paid properly.
    tipBlock = CreateAndProcessBlock({}, coinbaseKey);
    BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == payeeScript, "error: block not paying to proper MN");
    nextBlockHeight++;

    // Now let's push two valid winner payments and make every MN in the top ten vote for them (having more votes in mnwinnerA than in mnwinnerB).
    mnRank = mnodeman.GetMasternodeRanks(nextBlockHeight - 100);
    CScript firstRankedPayee = GetScriptForDestination(mnRank[0].second->pubKeyCollateralAddress.GetID());
    CScript secondRankedPayee = GetScriptForDestination(mnRank[1].second->pubKeyCollateralAddress.GetID());

    // Let's vote with the first 6 nodes for MN ranked 1
    // And with the last 4 nodes for MN ranked 2
    payeeScript = firstRankedPayee;
    for (int i=0; i<10; i++) {
        if (i > 5) {
            payeeScript = secondRankedPayee;
        }
        auto voterMn = findMNData(mnList, mnRank[i].second);
        CMasternode* pVoterMN = mnodeman.Find(voterMn.mn.vin.prevout);
        mnVinVoter = CTxIn(pVoterMN->vin);
        CValidationState stateInternal;
        BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, nextBlockHeight, payeeScript,
                                                             voterMn.data.mnPrivKey, voterMn.data.mnPubKey, stateInternal));
        BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
    }

    // Check the votes count for each mnwinner.
    CMasternodeBlockPayees blockPayees = masternodePayments.mapMasternodeBlocks.at(nextBlockHeight);
    BOOST_CHECK_MESSAGE(blockPayees.HasPayeeWithVotes(firstRankedPayee, 6), "first ranked payee with no enough votes");
    BOOST_CHECK_MESSAGE(blockPayees.HasPayeeWithVotes(secondRankedPayee, 4), "second ranked payee with no enough votes");

    // let's try to create a bad block paying to the second most voted MN.
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
    BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == firstRankedPayee, "error: block not paying to first ranked MN");
    nextBlockHeight++;

    //
    // Generate 125 blocks paying to different MNs to load the payments cache.
    for (int i = 0; i < 125; i++) {
        mnRank = mnodeman.GetMasternodeRanks(nextBlockHeight - 100);
        payeeScript = GetScriptForDestination(mnRank[0].second->pubKeyCollateralAddress.GetID());
        for (int j=0; j<7; j++) { // votes
            auto voterMn = findMNData(mnList, mnRank[j].second);
            CMasternode* pVoterMN = mnodeman.Find(voterMn.mn.vin.prevout);
            mnVinVoter = CTxIn(pVoterMN->vin);
            CValidationState stateInternal;
            BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, nextBlockHeight, payeeScript,
                                              voterMn.data.mnPrivKey, voterMn.data.mnPubKey, stateInternal));
            BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
        }
        // Create block and check that is being paid properly.
        tipBlock = CreateAndProcessBlock({}, coinbaseKey);
        BOOST_CHECK_MESSAGE(tipBlock.vtx[0]->vout.back().scriptPubKey == payeeScript, "error: block not paying to proper MN");
        nextBlockHeight++;
    }
    // Check chain height.
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Height();), nextBlockHeight - 1);

    // Let's now verify what happen if a previously paid MN goes offline but still have scheduled a payment in the future.
    // The current system allows it (up to a certain point) as payments are scheduled ahead of time and a MN can go down in the
    // [proposedWinnerHeightTime < currentHeight < currentHeight + 20] window.

    // 1) Schedule payment and vote for it with the first 6 MNs.
    mnRank = mnodeman.GetMasternodeRanks(nextBlockHeight - 100);
    MasternodeRef mnToPay = mnRank[0].second;
    payeeScript = GetScriptForDestination(mnToPay->pubKeyCollateralAddress.GetID());
    for (int i=0; i<6; i++) {
        auto voterMn = findMNData(mnList, mnRank[i].second);
        CMasternode* pVoterMN = mnodeman.Find(voterMn.mn.vin.prevout);
        mnVinVoter = CTxIn(pVoterMN->vin);
        CValidationState stateInternal;
        BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, nextBlockHeight, payeeScript,
                                          voterMn.data.mnPrivKey, voterMn.data.mnPubKey, stateInternal));
        BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
    }

    // 2) Remove payee MN from the MN list and try to emit a vote from MN7 to the same payee.
    // it should still be accepted because the MN was scheduled when it was online.
    mnodeman.Remove(mnToPay->vin.prevout);
    BOOST_CHECK_MESSAGE(!mnodeman.Find(mnToPay->vin.prevout), "error: removed MN is still available");

    // Now emit the vote from MN7
    auto voterMn = findMNData(mnList, mnRank[7].second);
    CMasternode* pVoterMN = mnodeman.Find(voterMn.mn.vin.prevout);
    mnVinVoter = CTxIn(pVoterMN->vin);
    CValidationState stateInternal;
    BOOST_CHECK(CreateMNWinnerPayment(mnVinVoter, nextBlockHeight, payeeScript,
                                      voterMn.data.mnPrivKey, voterMn.data.mnPubKey, stateInternal));
    BOOST_CHECK_MESSAGE(stateInternal.IsValid(), stateInternal.GetRejectReason());
}

BOOST_AUTO_TEST_SUITE_END()