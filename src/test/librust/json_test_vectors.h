// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "utilstrencodings.h"
#include "version.h"
#include "serialize.h"
#include "streams.h"

#include <univalue.h>

#include <boost/test/unit_test.hpp>

UniValue
read_json(const std::string& jsondata);

// #define PRINT_JSON 1

template<typename T>
void expect_deser_same(const T& expected)
{
    CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
    ss1 << expected;

    auto serialized_size = ss1.size();

    CDataStream ss1Copy = ss1;
    T object;
    ss1Copy >> object;

    CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
    ss2 << object;

    BOOST_ASSERT(serialized_size == ss2.size());
    BOOST_CHECK(memcmp(&*ss1.begin(), &*ss2.begin(), serialized_size) == 0);
}

template<typename T, typename U>
void expect_test_vector(T& v, const U& expected)
{
    expect_deser_same(expected);

    CDataStream ss1(SER_NETWORK, PROTOCOL_VERSION);
    ss1 << expected;

    #ifdef PRINT_JSON
    std::cout << "\t\"" ;
    std::cout << HexStr(ss1.begin(), ss1.end()) << "\",\n";
    #else
    std::string raw = v.get_str();
    CDataStream ss2(ParseHex(raw), SER_NETWORK, PROTOCOL_VERSION);

    BOOST_CHECK_EQUAL(ss1.size(), ss2.size());
    BOOST_CHECK(memcmp(&*ss1.begin(), &*ss2.begin(), ss1.size()) == 0);
    #endif
}
