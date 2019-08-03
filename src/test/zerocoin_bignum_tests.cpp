// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <iostream>

#include "libzerocoin/Commitment.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Accumulator.h"
#include "zpiv/zerocoin.h"


bool testRandKBitBignum(int k_bits)
{
    CBigNum x = CBigNum::randKBitBignum(k_bits);
    return (x.bitSize() <= k_bits);
}

bool testRandBignum(CBigNum limit)
{
    CBigNum x = CBigNum::randBignum(limit);
    return 0 <= x && x < limit;
}

BOOST_AUTO_TEST_SUITE(zerocoin_bignum_tests)

std::string zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
"4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
"6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
"7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
"8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
"31438167899885040445364023527381951378636564391212010397122822120720357";

std::string strHexModulus = "c7970ceedcc3b0754490201a7aa613cd73911081c790f5f1a8726f463550bb5b7ff0db8e1ea1189ec72f93d1650011bd721aeeacc2acde32a04107f0648c2813a31f5b0b7765ff8b44b4b6ffc93384b646eb09c7cf5e8592d40ea33c80039f35b4f14a04b51f7bfd781be4d1673164ba8eb991c2c4d730bbbe35f592bdef524af7e8daefd26c66fc02c479af89d64d373f442709439de66ceb955f3ea37d5159f6135809f85334b5cb1813addc80cd05609f10ac6a95ad65872c909525bdad32bc729592642920f24c61dc5b3c3b7923e56b16a4d9d373d8721f24a3fc0f1b3131f55615172866bccc30f95054c824e733a5eb6817f7bc16399d48c6361cc7e5";

BOOST_AUTO_TEST_CASE(bignum_setdecimal)
{
    CBigNum bnDec;
    bnDec.SetDec(zerocoinModulus);
    CBigNum bnHex;
    bnHex.SetHex(strHexModulus);
    BOOST_CHECK_MESSAGE(bnDec == bnHex, "CBigNum.SetDec() does not work correctly");
}

std::string negstrHexModulus = "-c7970ceedcc3b0754490201a7aa613cd73911081c790f5f1a8726f463550bb5b7ff0db8e1ea1189ec72f93d1650011bd721aeeacc2acde32a04107f0648c2813a31f5b0b7765ff8b44b4b6ffc93384b646eb09c7cf5e8592d40ea33c80039f35b4f14a04b51f7bfd781be4d1673164ba8eb991c2c4d730bbbe35f592bdef524af7e8daefd26c66fc02c479af89d64d373f442709439de66ceb955f3ea37d5159f6135809f85334b5cb1813addc80cd05609f10ac6a95ad65872c909525bdad32bc729592642920f24c61dc5b3c3b7923e56b16a4d9d373d8721f24a3fc0f1b3131f55615172866bccc30f95054c824e733a5eb6817f7bc16399d48c6361cc7e5";
std::string str_a = "775897c5463939bf29a02816aba7b1741162e1f6b052cd32fec36c44dfee7d4b5162de78bb0b448cb305b0a9bd7e006aec62d7c1e94a31003c2decbdc6fd7c9b261cb88801c51e7cee71a215ff113ccbd02069cf29671e6302944ca5780a2f626eb9046fa6872968addc93c74d09cf6b2872bc4c6bd08e89324cc7e9fb921488";
std::string str_b = "-775897c5463939bf29a02816aba7b1741162e1f6b052cd32fec36c44dfee7d4b5162de78bb0b448cb305b0a9bd7e006aec62d7c1e94a31003c2decbdc6fd7c9b261cb88801c51e7cee71a215ff113ccbd02069cf29671e6302944ca5780a2f626eb9046fa6872968addc93c74d09cf6b2872bc4c6bd08e89324cc7e9fb921488";

BOOST_AUTO_TEST_CASE(bignum_basic_tests)
{
    CBigNum bn, bn2;
    std::vector<unsigned char> vch;

    bn.SetHex(strHexModulus);
    vch = bn.getvch();
    bn2.setvch(vch);
    BOOST_CHECK_MESSAGE(bn2 == bn, "CBigNum.setvch() or CBigNum.getvch() does not work correctly");

    bn.SetHex(negstrHexModulus);
    vch = bn.getvch();
    bn2.setvch(vch);
    BOOST_CHECK_MESSAGE(bn2 == bn, "CBigNum.setvch() or CBigNum.getvch() does not work correctly");

    bn.SetHex(str_a);
    vch = bn.getvch();
    bn2.setvch(vch);
    BOOST_CHECK_MESSAGE(bn2 == bn, "CBigNum.setvch() or CBigNum.getvch() does not work correctly");

    bn.SetHex(str_b);
    vch = bn.getvch();
    bn2.setvch(vch);
    BOOST_CHECK_MESSAGE(bn2 == bn, "CBigNum.setvch() or CBigNum.getvch() does not work correctly");
}


BOOST_AUTO_TEST_CASE(bignum_random_generation_tests)
{
    for(int i=1; i<3000; i++) {
        BOOST_CHECK_MESSAGE(testRandKBitBignum(i), strprintf("CBigNum::randKBitBignum(%d) failed", i));
    }

    for(int i=1; i<3000; i++) {
        CBigNum x = 1 + CBigNum::randKBitBignum(i);
        BOOST_CHECK_MESSAGE(testRandBignum(x), strprintf("CBigNum::randBignum(x) failed with x=%s", x.ToString()));
    }
}

BOOST_AUTO_TEST_SUITE_END()
