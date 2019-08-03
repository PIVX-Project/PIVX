/**
* @file       Tests.cpp
*
* @brief      Test routines for Zerocoin.
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/
// Copyright (c) 2017-2018 The PIVX developers

#include <boost/test/unit_test.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
// #include <curses.h>
#include <exception>
#include "streams.h"
#include "libzerocoin/ParamGeneration.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/CoinSpend.h"
#include "libzerocoin/Accumulator.h"
#include "test_pivx.h"


#define COLOR_STR_GREEN   "\033[32m"
#define COLOR_STR_NORMAL  "\033[0m"
#define COLOR_STR_RED     "\033[31m"

#define TESTS_COINS_TO_ACCUMULATE   10
#define NON_PRIME_TESTS                100

// Global test counters
uint32_t    gNumTests        = 0;
uint32_t    gSuccessfulTests = 0;

// Proof size
uint32_t    gProofSize            = 0;
uint32_t    gCoinSize            = 0;
uint32_t    gSerialNumberSize    = 0;

// Global coin array
libzerocoin::PrivateCoin    *gCoins[TESTS_COINS_TO_ACCUMULATE];

// Global params
libzerocoin::ZerocoinParams *g_Params;

//////////
// Utility routines
//////////

void
LogTestResult(std::string testName, bool (*testPtr)())
{
    std::string colorGreen(COLOR_STR_GREEN);
    std::string colorNormal(COLOR_STR_NORMAL);
    std::string colorRed(COLOR_STR_RED);

    std::cout << "Testing if " << testName << "..." << std::endl;

    bool testResult = testPtr();

    if (testResult == true) {
        std::cout << "\t" << colorGreen << "[PASS]"  << colorNormal << std::endl;
        gSuccessfulTests++;
    } else {
        std::cout << colorRed << "\t[FAIL]" << colorNormal << std::endl;
    }

    gNumTests++;
}

CBigNum
GetTestModulus()
{
    static CBigNum testModulus(0);

    // TODO: should use a hard-coded RSA modulus for testing
    if (!testModulus) {
        CBigNum p, q;

        // Note: we are NOT using safe primes for testing because
        // they take too long to generate. Don't do this in real
        // usage. See the paramgen utility for better code.
        p = CBigNum::generatePrime(1024, false);
        q = CBigNum::generatePrime(1024, false);
        testModulus = p * q;
    }

    return testModulus;
}

//////////
// Test routines
//////////

bool
Test_GenRSAModulus()
{
    CBigNum result = GetTestModulus();

    if (!result) {
        return false;
    }
    else {
        return true;
    }
}

bool
Test_CalcParamSizes()
{
    bool result = true;
#if 0

    uint32_t pLen, qLen;

    try {
        calculateGroupParamLengths(4000, 80, &pLen, &qLen);
        if (pLen < 1024 || qLen < 256) {
            result = false;
        }
        calculateGroupParamLengths(4000, 96, &pLen, &qLen);
        if (pLen < 2048 || qLen < 256) {
            result = false;
        }
        calculateGroupParamLengths(4000, 112, &pLen, &qLen);
        if (pLen < 3072 || qLen < 320) {
            result = false;
        }
        calculateGroupParamLengths(4000, 120, &pLen, &qLen);
        if (pLen < 3072 || qLen < 320) {
            result = false;
        }
        calculateGroupParamLengths(4000, 128, &pLen, &qLen);
        if (pLen < 3072 || qLen < 320) {
            result = false;
        }
    } catch (exception &e) {
        result = false;
    }
#endif

    return result;
}

bool
Test_GenerateGroupParams()
{
    uint32_t pLen = 1024, qLen = 256, count;
    libzerocoin::IntegerGroupParams group;

    for (count = 0; count < 1; count++) {

        try {
            group = libzerocoin::deriveIntegerGroupParams(libzerocoin::calculateSeed(GetTestModulus(), "test", ZEROCOIN_DEFAULT_SECURITYLEVEL, "TEST GROUP"), pLen, qLen);
        } catch (std::runtime_error e) {
            std::cout << "Caught exception " << e.what() << std::endl;
            return false;
        }

        // Now perform some simple tests on the resulting parameters
        if ((uint32_t)group.groupOrder.bitSize() < qLen || (uint32_t)group.modulus.bitSize() < pLen) {
            return false;
        }

        CBigNum c = group.g.pow_mod(group.groupOrder, group.modulus);
        //cout << "g^q mod p = " << c << std::endl;
        if (!(c.isOne())) return false;

        // Try at multiple parameter sizes
        pLen = pLen * 1.5;
        qLen = qLen * 1.5;
    }

    return true;
}

bool
Test_ParamGen()
{
    bool result = true;

    try {
        // Instantiating testParams runs the parameter generation code
        libzerocoin::ZerocoinParams testParams(GetTestModulus(),ZEROCOIN_DEFAULT_SECURITYLEVEL);
    } catch (std::runtime_error e) {
        std::cout << e.what() << std::endl;
        result = false;
    }

    return result;
}

bool
Test_Accumulator()
{
    // This test assumes a list of coins were generated during
    // the Test_MintCoin() test.
    if (gCoins[0] == NULL) {
        return false;
    }
    try {
        // Accumulate the coin list from first to last into one accumulator
        libzerocoin::Accumulator accOne(&g_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accTwo(&g_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accThree(&g_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accFour(&g_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::AccumulatorWitness wThree(g_Params, accThree, gCoins[0]->getPublicCoin());

        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            accOne += gCoins[i]->getPublicCoin();
            accTwo += gCoins[TESTS_COINS_TO_ACCUMULATE - (i+1)]->getPublicCoin();
            accThree += gCoins[i]->getPublicCoin();
            wThree += gCoins[i]->getPublicCoin();
            if(i != 0) {
                accFour += gCoins[i]->getPublicCoin();
            }
        }

        // Compare the accumulated results
        if (accOne.getValue() != accTwo.getValue() || accOne.getValue() != accThree.getValue()) {
            std::cout << "Accumulators don't match" << std::endl;
            return false;
        }

        if(accFour.getValue() != wThree.getValue()) {
            std::cout << "Witness math not working," << std::endl;
            return false;
        }

        // Verify that the witness is correct
        if (!wThree.VerifyWitness(accThree, gCoins[0]->getPublicCoin()) ) {
            std::cout << "Witness not valid" << std::endl;
            return false;
        }

        // Serialization test: see if we can serialize the accumulator
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << accOne;

        // Deserialize it into a new object
        libzerocoin::Accumulator newAcc(g_Params, ss);

        // Compare the results
        if (accOne.getValue() != newAcc.getValue()) {
            return false;
        }

    } catch (std::runtime_error e) {
        return false;
    }

    return true;
}

bool
Test_EqualityPoK()
{
    // Run this test 10 times
    for (uint32_t i = 0; i < 10; i++) {
        try {
            // Generate a random integer "val"
            CBigNum val = CBigNum::randBignum(g_Params->coinCommitmentGroup.groupOrder);

            // Manufacture two commitments to "val", both
            // under different sets of parameters
            libzerocoin::Commitment one(&g_Params->accumulatorParams.accumulatorPoKCommitmentGroup, val);

            libzerocoin::Commitment two(&g_Params->serialNumberSoKCommitmentGroup, val);

            // Now generate a proof of knowledge that "one" and "two" are
            // both commitments to the same value
            libzerocoin::CommitmentProofOfKnowledge pok(&g_Params->accumulatorParams.accumulatorPoKCommitmentGroup,
                                           &g_Params->serialNumberSoKCommitmentGroup,
                                           one, two);

            // Serialize the proof into a stream
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << pok;

            // Deserialize back into a PoK object
            libzerocoin::CommitmentProofOfKnowledge newPok(&g_Params->accumulatorParams.accumulatorPoKCommitmentGroup,
                                              &g_Params->serialNumberSoKCommitmentGroup,
                                              ss);

            if (newPok.Verify(one.getCommitmentValue(), two.getCommitmentValue()) != true) {
                return false;
            }

            // Just for fun, deserialize the proof a second time
            CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
            ss2 << pok;

            // This time tamper with it, then deserialize it back into a PoK
            ss2[15] = 0;
            libzerocoin::CommitmentProofOfKnowledge newPok2(&g_Params->accumulatorParams.accumulatorPoKCommitmentGroup,
                                               &g_Params->serialNumberSoKCommitmentGroup,
                                               ss2);

            // If the tampered proof verifies, that's a failure!
            if (newPok2.Verify(one.getCommitmentValue(), two.getCommitmentValue()) == true) {
                return false;
            }

        } catch (std::runtime_error &e) {
            return false;
        }
    }

    return true;
}

bool
Test_MintCoin()
{
    gCoinSize = 0;

    try {
        // Generate a list of coins
        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            gCoins[i] = new libzerocoin::PrivateCoin(g_Params,libzerocoin::CoinDenomination::ZQ_ONE);

            libzerocoin::PublicCoin pc = gCoins[i]->getPublicCoin();
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << pc;
            gCoinSize += ss.size();
        }

        gCoinSize /= TESTS_COINS_TO_ACCUMULATE;

    } catch (std::exception &e) {
        return false;
    }

    return true;
}

bool Test_InvalidCoin()
{
    CBigNum coinValue;

    try {
        // Pick a random non-prime CBigNum
        for (uint32_t i = 0; i < NON_PRIME_TESTS; i++) {
            coinValue = CBigNum::randBignum(g_Params->coinCommitmentGroup.modulus);
            coinValue = coinValue * 2;
            if (!coinValue.isPrime()) break;
        }

        libzerocoin::PublicCoin pubCoin(g_Params);
        if (pubCoin.validate()) {
            // A blank coin should not be valid!
            return false;
        }

        libzerocoin::PublicCoin pubCoin2(g_Params, coinValue, libzerocoin::ZQ_ONE);
        if (pubCoin2.validate()) {
            // A non-prime coin should not be valid!
            return false;
        }

        libzerocoin::PublicCoin pubCoin3 = pubCoin2;
        if (pubCoin2.validate()) {
            // A copy of a non-prime coin should not be valid!
            return false;
        }

        // Serialize and deserialize the coin
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << pubCoin;
        libzerocoin::PublicCoin pubCoin4(g_Params, ss);
        if (pubCoin4.validate()) {
            // A deserialized copy of a non-prime coin should not be valid!
            return false;
        }

    } catch (std::runtime_error &e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool
Test_MintAndSpend()
{
    try {
        // This test assumes a list of coins were generated in Test_MintCoin()
        if (gCoins[0] == NULL)
        {
            // No coins: mint some.
            Test_MintCoin();
            if (gCoins[0] == NULL) {
                return false;
            }
        }

        // Accumulate the list of generated coins into a fresh accumulator.
        // The first one gets marked as accumulated for a witness, the
        // others just get accumulated normally.
        libzerocoin::Accumulator acc(&g_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::AccumulatorWitness wAcc(g_Params, acc, gCoins[0]->getPublicCoin());

        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            acc += gCoins[i]->getPublicCoin();
            wAcc +=gCoins[i]->getPublicCoin();
        }

        // Now spend the coin
        //SpendMetaData m(1,1);
        CDataStream cc(SER_NETWORK, PROTOCOL_VERSION);
        cc << *gCoins[0];
        libzerocoin::PrivateCoin myCoin(g_Params,cc);

        libzerocoin::CoinSpend spend(g_Params, g_Params, myCoin, acc, 0, wAcc, 0, libzerocoin::SpendType::SPEND);
        spend.Verify(acc);

        // Serialize the proof and deserialize into newSpend
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << spend;
        gProofSize = ss.size();
        libzerocoin::CoinSpend newSpend(g_Params, g_Params, ss);

        // See if we can verify the deserialized proof (return our result)
        bool ret =  newSpend.Verify(acc);

        // Extract the serial number
        CBigNum serialNumber = newSpend.getCoinSerialNumber();
        gSerialNumberSize = ceil((double)serialNumber.bitSize() / 8.0);

        return ret;
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    return false;
}

void
Test_RunAllTests()
{
    // Make a new set of parameters from a random RSA modulus
    g_Params = new libzerocoin::ZerocoinParams(GetTestModulus());

    gNumTests = gSuccessfulTests = gProofSize = 0;
    for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        gCoins[i] = NULL;
    }

    // Run through all of the Zerocoin tests
    LogTestResult("an RSA modulus can be generated", Test_GenRSAModulus);
    LogTestResult("parameter sizes are correct", Test_CalcParamSizes);
    LogTestResult("group/field parameters can be generated", Test_GenerateGroupParams);
    LogTestResult("parameter generation is correct", Test_ParamGen);
    LogTestResult("coins can be minted", Test_MintCoin);
    LogTestResult("invalid coins will be rejected", Test_InvalidCoin);
    LogTestResult("the accumulator works", Test_Accumulator);
    LogTestResult("the commitment equality PoK works", Test_EqualityPoK);
    LogTestResult("a minted coin can be spent", Test_MintAndSpend);

    std::cout << std::endl << "Average coin size is " << gCoinSize << " bytes." << std::endl;
    std::cout << "Serial number size is " << gSerialNumberSize << " bytes." << std::endl;
    std::cout << "Spend proof size is " << gProofSize << " bytes." << std::endl;

    // Summarize test results
    if (gSuccessfulTests < gNumTests) {
        std::cout << std::endl << "ERROR: SOME TESTS FAILED" << std::endl;
    }

    // Clear any generated coins
    for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        delete gCoins[i];
    }

    std::cout << std::endl << gSuccessfulTests << " out of " << gNumTests << " tests passed." << std::endl << std::endl;
    delete g_Params;
}

BOOST_FIXTURE_TEST_SUITE(libzerocoin, TestingSetup)
BOOST_AUTO_TEST_CASE(libzerocoin_tests)
{
    std::cout << "libzerocoin v" << ZEROCOIN_VERSION_STRING << " test utility." << std::endl << std::endl;

    Test_RunAllTests();
}
BOOST_AUTO_TEST_SUITE_END()
