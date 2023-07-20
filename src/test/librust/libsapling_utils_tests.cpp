// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"

#include "sapling/sapling_util.h"
#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(sapling_utils_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(saplingConvertBytesVectorToVector) {
    std::vector<unsigned char> bytes = {0x00, 0x01, 0x03, 0x12, 0xFF};
    std::vector<bool> expected_bits = {
            // 0x00
            0, 0, 0, 0, 0, 0, 0, 0,
            // 0x01
            0, 0, 0, 0, 0, 0, 0, 1,
            // 0x03
            0, 0, 0, 0, 0, 0, 1, 1,
            // 0x12
            0, 0, 0, 1, 0, 0, 1, 0,
            // 0xFF
            1, 1, 1, 1, 1, 1, 1, 1
    };
    BOOST_CHECK(convertBytesVectorToVector(bytes) == expected_bits);
}

BOOST_AUTO_TEST_CASE(saplingConvertVectorToInt) {

    BOOST_CHECK(convertVectorToInt({0}) == 0);
    BOOST_CHECK(convertVectorToInt({1}) == 1);
    BOOST_CHECK(convertVectorToInt({0,1}) == 1);
    BOOST_CHECK(convertVectorToInt({1,0}) == 2);
    BOOST_CHECK(convertVectorToInt({1,1}) == 3);
    BOOST_CHECK(convertVectorToInt({1,0,0}) == 4);
    BOOST_CHECK(convertVectorToInt({1,0,1}) == 5);
    BOOST_CHECK(convertVectorToInt({1,1,0}) == 6);

    BOOST_CHECK_THROW(convertVectorToInt(std::vector<bool>(100)), std::length_error);

    {
        std::vector<bool> v(63, 1);
        BOOST_CHECK(convertVectorToInt(v) == 0x7fffffffffffffff);
    }

    {
        std::vector<bool> v(64, 1);
        BOOST_CHECK(convertVectorToInt(v) == 0xffffffffffffffff);
    }

}

BOOST_AUTO_TEST_SUITE_END()
