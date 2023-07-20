// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"

#include "arith_uint256.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>
#include <librustzcash.h>

BOOST_FIXTURE_TEST_SUITE(pedersen_hash_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pedersen_hash_testvectors)
{
    // Good scenario check
    const uint256 a = uint256S("87a086ae7d2252d58729b30263fb7b66308bf94ef59a76c9c86e7ea016536505");
    const uint256 b = uint256S("a75b84a125b2353da7e8d96ee2a15efe4de23df9601b9d9564ba59de57130406");
    uint256 result;

    librustzcash_merkle_hash(25, a.begin(), b.begin(), result.begin());

    uint256 expected_result = uint256S("5bf43b5736c19b714d1f462c9d22ba3492c36e3d9bbd7ca24d94b440550aa561");

    BOOST_CHECK(result == expected_result);

    // Simple bad scenario check
    const uint256& c = ArithToUint256(ARITH_UINT256_ONE + UintToArith256(a));
    result.SetNull();
    librustzcash_merkle_hash(25, c.begin(), b.begin(), result.begin());
    BOOST_CHECK(result != expected_result);
}

BOOST_AUTO_TEST_SUITE_END()
