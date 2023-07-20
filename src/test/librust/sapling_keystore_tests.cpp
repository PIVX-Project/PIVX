// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"
#include "test/librust/utiltest.h"

#include "sapling/address.h"
#include "sapling/sapling_util.h"

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include "data/sapling_key_components.json.h"

// In script_tests.cpp
extern UniValue read_json(const std::string& jsondata);

BOOST_FIXTURE_TEST_SUITE(sapling_keystore_tests, BasicTestingSetup)


BOOST_AUTO_TEST_CASE(saplingKeys) {
    // ["sk, ask, nsk, ovk, ak, nk, ivk, default_d, default_pk_d, note_v, note_r, note_cm, note_pos, note_nf"],
    UniValue sapling_keys = read_json(std::string(json_tests::sapling_key_components, json_tests::sapling_key_components + sizeof(json_tests::sapling_key_components)));

    // Skipping over comments in sapling_key_components.json file
    for (size_t i = 2; i < 12; i++) {
        uint256 skSeed, ask, nsk, ovk, ak, nk, ivk;
        skSeed.SetHex(sapling_keys[i][0].getValStr());
        ask.SetHex(sapling_keys[i][1].getValStr());
        nsk.SetHex(sapling_keys[i][2].getValStr());
        ovk.SetHex(sapling_keys[i][3].getValStr());
        ak.SetHex(sapling_keys[i][4].getValStr());
        nk.SetHex(sapling_keys[i][5].getValStr());
        ivk.SetHex(sapling_keys[i][6].getValStr());

        diversifier_t default_d;
        std::copy_n(ParseHex(sapling_keys[i][7].getValStr()).begin(), 11, default_d.begin());

        uint256 default_pk_d;
        default_pk_d.SetHex(sapling_keys[i][8].getValStr());

        auto sk = libzcash::SaplingSpendingKey(skSeed);

        // Check that expanded spending key from primitives and from sk are the same
        auto exp_sk_2 = libzcash::SaplingExpandedSpendingKey(ask, nsk, ovk);
        auto exp_sk = sk.expanded_spending_key();
        BOOST_CHECK(exp_sk == exp_sk_2);

        // Check that full viewing key derived from sk and expanded sk are the same
        auto full_viewing_key = sk.full_viewing_key();
        BOOST_CHECK(full_viewing_key == exp_sk.full_viewing_key());

        // Check that full viewing key from primitives and from sk are the same
        auto full_viewing_key_2 = libzcash::SaplingFullViewingKey(ak, nk, ovk);
        BOOST_CHECK(full_viewing_key == full_viewing_key_2);

        // Check that incoming viewing key from primitives and from sk are the same
        auto in_viewing_key = full_viewing_key.in_viewing_key();
        auto in_viewing_key_2 = libzcash::SaplingIncomingViewingKey(ivk);
        BOOST_CHECK(in_viewing_key == in_viewing_key_2);

        // Check that the default address from primitives and from sk method are the same
        auto default_addr = sk.default_address();
        auto addrOpt2 = in_viewing_key.address(default_d);
        BOOST_CHECK(addrOpt2);
        auto default_addr_2 = addrOpt2.value();
        BOOST_CHECK(default_addr == default_addr_2);

        auto default_addr_3 = libzcash::SaplingPaymentAddress(default_d, default_pk_d);
        BOOST_CHECK(default_addr_2 == default_addr_3);
        BOOST_CHECK(default_addr == default_addr_3);
    }
}


BOOST_AUTO_TEST_CASE(StoreAndRetrieveSaplingSpendingKey) {
    CBasicKeyStore keyStore;
    libzcash::SaplingExtendedSpendingKey skOut;
    libzcash::SaplingExtendedFullViewingKey extfvkOut;
    libzcash::SaplingIncomingViewingKey ivkOut;

    std::vector<unsigned char, secure_allocator<unsigned char>> rawSeed(32);
    HDSeed seed(rawSeed);
    auto sk = libzcash::SaplingExtendedSpendingKey::Master(seed);
    auto extfvk = sk.ToXFVK();
    auto ivk = extfvk.fvk.in_viewing_key();
    auto addr = sk.DefaultAddress();

    // Sanity-check: we can't get a key we haven't added
    BOOST_CHECK(!keyStore.HaveSaplingSpendingKey(extfvk));
    BOOST_CHECK(!keyStore.GetSaplingSpendingKey(extfvk, skOut));
    // Sanity-check: we can't get a full viewing key we haven't added
    BOOST_CHECK(!keyStore.HaveSaplingFullViewingKey(ivk));
    BOOST_CHECK(!keyStore.GetSaplingFullViewingKey(ivk, extfvkOut));
    // Sanity-check: we can't get an incoming viewing key we haven't added
    BOOST_CHECK(!keyStore.HaveSaplingIncomingViewingKey(addr));
    BOOST_CHECK(!keyStore.GetSaplingIncomingViewingKey(addr, ivkOut));

    // When we specify the default address, we get the full mapping
    keyStore.AddSaplingSpendingKey(sk);
    BOOST_CHECK(keyStore.HaveSaplingSpendingKey(extfvk));
    BOOST_CHECK(keyStore.GetSaplingSpendingKey(extfvk, skOut));
    BOOST_CHECK(keyStore.HaveSaplingFullViewingKey(ivk));
    BOOST_CHECK(keyStore.GetSaplingFullViewingKey(ivk, extfvkOut));
    BOOST_CHECK(keyStore.HaveSaplingIncomingViewingKey(addr));
    BOOST_CHECK(keyStore.GetSaplingIncomingViewingKey(addr, ivkOut));
    BOOST_CHECK(sk == skOut);
    BOOST_CHECK(extfvk == extfvkOut);
    BOOST_CHECK(ivk == ivkOut);
}

BOOST_AUTO_TEST_CASE(StoreAndRetrieveSaplingFullViewingKey) {
    CBasicKeyStore keyStore;
    libzcash::SaplingExtendedSpendingKey skOut;
    libzcash::SaplingExtendedFullViewingKey extfvkOut;
    libzcash::SaplingIncomingViewingKey ivkOut;

    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto ivk = extfvk.fvk.in_viewing_key();
    auto addr = sk.DefaultAddress();

    // Sanity-check: we can't get a full viewing key we haven't added
    BOOST_CHECK(!keyStore.HaveSaplingFullViewingKey(ivk));
    BOOST_CHECK(!keyStore.GetSaplingFullViewingKey(ivk, extfvkOut));

    // and we shouldn't have a spending key or incoming viewing key either
    BOOST_CHECK(!keyStore.HaveSaplingSpendingKey(extfvk));
    BOOST_CHECK(!keyStore.GetSaplingSpendingKey(extfvk, skOut));
    BOOST_CHECK(!keyStore.HaveSaplingIncomingViewingKey(addr));
    BOOST_CHECK(!keyStore.GetSaplingIncomingViewingKey(addr, ivkOut));

    // and we can't find the default address in our list of addresses
    std::set<libzcash::SaplingPaymentAddress> addresses;
    keyStore.GetSaplingPaymentAddresses(addresses);
    BOOST_CHECK(!addresses.count(addr));

    // When we add the full viewing key, we should have it
    keyStore.AddSaplingFullViewingKey(extfvk);
    BOOST_CHECK(keyStore.HaveSaplingFullViewingKey(ivk));
    BOOST_CHECK(keyStore.GetSaplingFullViewingKey(ivk, extfvkOut));
    BOOST_CHECK(extfvk == extfvkOut);

    // We should still not have the spending key...
    BOOST_CHECK(!keyStore.HaveSaplingSpendingKey(extfvk));
    BOOST_CHECK(!keyStore.GetSaplingSpendingKey(extfvk, skOut));

    // ... but we should have an incoming viewing key
    BOOST_CHECK(keyStore.HaveSaplingIncomingViewingKey(addr));
    BOOST_CHECK(keyStore.GetSaplingIncomingViewingKey(addr, ivkOut));
    BOOST_CHECK(ivk == ivkOut);

    // ... and we should find the default address in our list of addresses
    addresses.clear();
    keyStore.GetSaplingPaymentAddresses(addresses);
    BOOST_CHECK(addresses.count(addr));
}

BOOST_AUTO_TEST_SUITE_END()
