// Copyright (c) 2019-2020 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"
#include "bls/bls_ies.h"
#include "bls/key_io.h"
#include "bls/bls_worker.h"
#include "bls/bls_wrapper.h"
#include "random.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bls_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bls_sig_tests)
{
    CBLSSecretKey sk1, sk2;
    sk1.MakeNewKey();
    sk2.MakeNewKey();

    uint256 msgHash1 = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    uint256 msgHash2 = uint256S("0000000000000000000000000000000000000000000000000000000000000002");

    auto sig1 = sk1.Sign(msgHash1);
    auto sig2 = sk2.Sign(msgHash1);

    BOOST_CHECK(sig1.VerifyInsecure(sk1.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig1.VerifyInsecure(sk1.GetPublicKey(), msgHash2));

    BOOST_CHECK(sig2.VerifyInsecure(sk2.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig2.VerifyInsecure(sk2.GetPublicKey(), msgHash2));

    BOOST_CHECK(!sig1.VerifyInsecure(sk2.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig1.VerifyInsecure(sk2.GetPublicKey(), msgHash2));
    BOOST_CHECK(!sig2.VerifyInsecure(sk1.GetPublicKey(), msgHash1));
    BOOST_CHECK(!sig2.VerifyInsecure(sk1.GetPublicKey(), msgHash2));
}

static BLSIdVector GetRandomBLSIds(size_t n)
{
    BLSIdVector v;
    for (size_t i = 0; i < n; i++) {
        v.emplace_back(GetRandHash());
    }
    return v;
}

std::vector<size_t> GetRandomElements(size_t m, size_t n)
{
    assert(m <= n);
    std::vector<size_t> idxs;
    for (size_t i = 0; i < n; i++) {
        idxs.emplace_back(i);
    }
    Shuffle(idxs.begin(), idxs.end(), FastRandomContext());
    return std::vector<size_t>(idxs.begin(), idxs.begin() + m);
}

struct Member
{
    CBLSId id;
    BLSVerificationVectorPtr vecP;
    CBLSIESMultiRecipientObjects<CBLSSecretKey> contributions;
    CBLSSecretKey skShare;

    // member (operator) keys for encryption/decryption of contributions
    CBLSSecretKey sk;
    CBLSPublicKey pk;

    Member(const CBLSId& _id): id(_id)
    {
        sk.MakeNewKey();
        pk = sk.GetPublicKey();
    }
};

BOOST_AUTO_TEST_CASE(dkg)
{
    CBLSWorker worker;
    const size_t N = 40;     // quorum size
    const size_t M = 30;     // threshold

    worker.Start();

    // Create N Members first
    const BLSIdVector& ids = GetRandomBLSIds(N);
    std::vector<Member> quorum;
    for (const auto& id : ids) {
        quorum.emplace_back(Member(id));
    }

    // Then generate contributions for each one
    for (Member& m : quorum) {
        // Generate contributions (plain text)
        BLSSecretKeyVector pt_contributions;
        worker.GenerateContributions((int)M, ids, m.vecP, pt_contributions);
        BOOST_CHECK_EQUAL(m.vecP->size(), M);
        BOOST_CHECK_EQUAL(pt_contributions.size(), N);
        // Init encrypted multi-recipient object
        m.contributions.InitEncrypt(N);
        for (size_t j = 0; j < N; j++) {
            const CBLSSecretKey& plaintext = pt_contributions[j];
            // Verify contribution against verification vector
            BOOST_CHECK(worker.VerifyContributionShare(ids[j], m.vecP, plaintext));
            // Encrypt each contribution with the recipient pk
            BOOST_CHECK(m.contributions.Encrypt(j, quorum[j].pk, plaintext, PROTOCOL_VERSION));
        }
    }

    // Aggregate received contributions for each Member to produce key shares
    for (size_t i = 0; i < N; i++) {
        Member& m = quorum[i];
        // Decrypt contributions received by m with m's secret key
        BLSSecretKeyVector rcvSkContributions;
        for (size_t j = 0; j < N; j++) {
            CBLSSecretKey contribution;
            BOOST_CHECK(quorum[j].contributions.Decrypt(i, m.sk, contribution, PROTOCOL_VERSION));
            rcvSkContributions.emplace_back(std::move(contribution));
        }
        m.skShare = worker.AggregateSecretKeys(rcvSkContributions);
        // Recover public key share for m, and check against the secret key share
        BLSPublicKeyVector rcvPkContributions;
        for (size_t j = 0; j < N; j++) {
            CBLSPublicKey pkContribution = worker.BuildPubKeyShare(quorum[j].vecP, m.id);
            // This is implied by VerifyContributionShare, but let's double check
            BOOST_CHECK(rcvSkContributions[j].GetPublicKey() == pkContribution);
            rcvPkContributions.emplace_back(pkContribution);
        }
        CBLSPublicKey pkShare = worker.AggregatePublicKeys(rcvPkContributions);
        BOOST_CHECK(m.skShare.GetPublicKey() == pkShare);
    }

    // Each member signs a message with its key share producing a signature share
    const uint256& msg = GetRandHash();
    BLSSignatureVector allSigShares;
    for (const Member& m : quorum) {
        allSigShares.emplace_back(m.skShare.Sign(msg));
    }

    // Pick M (random) key shares and recover threshold secret/public key
    const auto& idxs = GetRandomElements(M, N);
    BLSSecretKeyVector skShares;
    BLSIdVector random_ids;
    for (size_t i : idxs) {
        skShares.emplace_back(quorum[i].skShare);
        random_ids.emplace_back(quorum[i].id);
    }
    CBLSSecretKey thresholdSk;
    BOOST_CHECK(thresholdSk.Recover(skShares, random_ids));
    const CBLSPublicKey& thresholdPk = thresholdSk.GetPublicKey();

    // Check that the recovered threshold public key equals the verification
    // vector free coefficient
    std::vector<BLSVerificationVectorPtr> v;
    for (const Member& m : quorum) v.emplace_back(m.vecP);
    CBLSPublicKey pk = worker.BuildQuorumVerificationVector(v)->at(0);
    BOOST_CHECK(pk == thresholdPk);

    // Pick M (random, different BLSids than before) signature shares, and recover
    // the threshold signature
    const auto& idxs2 = GetRandomElements(M, N);
    BLSSignatureVector sigShares;
    BLSIdVector random_ids2;
    for (size_t i : idxs2) {
        sigShares.emplace_back(allSigShares[i]);
        random_ids2.emplace_back(quorum[i].id);
    }
    CBLSSignature thresholdSig;
    BOOST_CHECK(thresholdSig.Recover(sigShares, random_ids2));

    // Verify threshold signature against threshold public key
    BOOST_CHECK(thresholdSig.VerifyInsecure(thresholdPk, msg));

    // Now replace a signature share with an invalid signature, recover the threshold
    // signature again, and check that verification fails with the threshold public key
    CBLSSecretKey dummy_sk;
    dummy_sk.MakeNewKey();
    CBLSSignature dummy_sig = dummy_sk.Sign(msg);
    BOOST_CHECK(dummy_sig != sigShares[0]);
    sigShares[0] = dummy_sig;
    BOOST_CHECK(thresholdSig.Recover(sigShares, random_ids2));
    BOOST_CHECK(!thresholdSig.VerifyInsecure(thresholdPk, msg));

    worker.Stop();
}

BOOST_AUTO_TEST_CASE(bls_ies_tests)
{
    // Test basic encryption and decryption of the BLS Integrated Encryption Scheme.
    CBLSSecretKey aliceSk;
    aliceSk.MakeNewKey();
    const CBLSPublicKey alicePk = aliceSk.GetPublicKey();
    BOOST_CHECK(aliceSk.IsValid());

    CBLSSecretKey bobSk;
    bobSk.MakeNewKey();
    const CBLSPublicKey bobPk = bobSk.GetPublicKey();
    BOOST_CHECK(bobSk.IsValid());

    // Encrypt a std::string object
    CBLSIESEncryptedObject<std::string> iesEnc;

    // Since no pad is allowed, serialized length must be a multiple of AES_BLOCKSIZE (16)
    BOOST_CHECK(!iesEnc.Encrypt(bobPk, "message of length 20", PROTOCOL_VERSION));

    // Message of valid length (15 + 1 byte for the total len in serialization)
    std::string message = ".mess of len 15";
    BOOST_CHECK(iesEnc.Encrypt(bobPk, message, PROTOCOL_VERSION));

    // valid decryption.
    std::string decrypted_message;
    BOOST_CHECK(iesEnc.Decrypt(bobSk, decrypted_message, PROTOCOL_VERSION));
    BOOST_CHECK_EQUAL(decrypted_message, message);

    // Invalid decryption sk
    std::string decrypted_message2;
    iesEnc.Decrypt(aliceSk, decrypted_message2, PROTOCOL_VERSION);
    BOOST_CHECK(decrypted_message2 != message);

    // Invalid ephemeral pubkey
    decrypted_message2.clear();
    auto iesEphemeralPk = iesEnc.ephemeralPubKey;
    iesEnc.ephemeralPubKey = alicePk;
    iesEnc.Decrypt(bobSk, decrypted_message2, PROTOCOL_VERSION);
    BOOST_CHECK(decrypted_message2 != message);
    iesEnc.ephemeralPubKey = iesEphemeralPk;

    // Invalid iv
    decrypted_message2.clear();
    GetRandBytes(iesEnc.iv, sizeof(iesEnc.iv));
    iesEnc.Decrypt(bobSk, decrypted_message2, PROTOCOL_VERSION);
    BOOST_CHECK(decrypted_message2 != message);
}

template<typename BLSKey>
BLSKey FromHex(const std::string& str)
{
    BLSKey k;
    k.SetByteVector(ParseHex(str));
    return k;
}

BOOST_AUTO_TEST_CASE(bls_sk_io_tests)
{
    const auto& params = Params();

    CBLSSecretKey sk = FromHex<CBLSSecretKey>("2eb071f4c520b3102e8cb9f520783da252d33993dba0313b501d69d113af9d39");
    BOOST_ASSERT(sk.IsValid());

    // Basic encoding-decoding roundtrip
    std::string encodedSk = bls::EncodeSecret(params, sk);
    auto opSk2 = bls::DecodeSecret(params, encodedSk);
    BOOST_CHECK(opSk2 != nullopt);
    CBLSSecretKey sk2 = *opSk2;
    BOOST_CHECK(sk == sk2);

    // Invalid sk, one extra char
    encodedSk.push_back('f');
    auto opSk3 = bls::DecodeSecret(params, encodedSk);
    BOOST_CHECK(opSk3 == nullopt);

    // Invalid sk, one less char
    encodedSk.pop_back();
    encodedSk.pop_back();
    auto opSk4 = bls::DecodeSecret(params, encodedSk);
    BOOST_CHECK(opSk4 == nullopt);
}

BOOST_AUTO_TEST_CASE(bls_pk_io_tests)
{
    const auto& params = Params();

    CBLSPublicKey pk = FromHex<CBLSPublicKey>("901138a12a352c7e30408c071b1ec097f32ab735a12c8dbb43c637612a3f805668a6bb73894982366d287cf0b02aaf5b");
    BOOST_ASSERT(pk.IsValid());

    // Basic encoding-decoding roundtrip
    std::string encodedPk = bls::EncodePublic(params, pk);
    auto opPk2 = bls::DecodePublic(params, encodedPk);
    BOOST_CHECK(opPk2 != nullopt);
    CBLSPublicKey pk2 = *opPk2;
    BOOST_CHECK(pk == pk2);

    // Invalid pk, one extra char
    encodedPk.push_back('f');
    auto oppk3 = bls::DecodePublic(params, encodedPk);
    BOOST_CHECK(oppk3 == nullopt);

    // Invalid pk, one less char
    encodedPk.pop_back();
    encodedPk.pop_back();
    auto oppk4 = bls::DecodePublic(params, encodedPk);
    BOOST_CHECK(oppk4 == nullopt);
}

BOOST_AUTO_TEST_SUITE_END()
