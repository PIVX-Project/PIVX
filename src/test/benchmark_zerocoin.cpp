/**
 * @file       Benchmark.cpp
 *
 * @brief      Benchmarking tests for Zerocoin.
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
// #include <curses.h>
#include <exception>
#include <cstdlib>
#include <sys/time.h>
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

#define TESTS_COINS_TO_ACCUMULATE   50

// Global test counters
uint32_t    ggNumTests        = 0;
uint32_t    ggSuccessfulTests = 0;

// Global coin array
libzerocoin::PrivateCoin    *ggCoins[TESTS_COINS_TO_ACCUMULATE];

// Global params
libzerocoin::ZerocoinParams *gg_Params;

//////////
// Utility routines
//////////

class Timer
{
    timeval timer[2];

public:

    timeval start()
    {
        gettimeofday(&this->timer[0], NULL);
        return this->timer[0];
    }

    timeval stop()
    {
        gettimeofday(&this->timer[1], NULL);
        return this->timer[1];
    }

    int duration() const
    {
        int secs(this->timer[1].tv_sec - this->timer[0].tv_sec);
        int usecs(this->timer[1].tv_usec - this->timer[0].tv_usec);

        if(usecs < 0)
        {
            --secs;
            usecs += 1000000;
        }

        return static_cast<int>(secs * 1000 + usecs / 1000.0 + 0.5);
    }
};

// Global timer
Timer timer;

void
gLogTestResult(std::string testName, bool (*testPtr)())
{
    std::string colorGreen(COLOR_STR_GREEN);
    std::string colorNormal(COLOR_STR_NORMAL);
    std::string colorRed(COLOR_STR_RED);

    std::cout << "Testing if " << testName << "..." << std::endl;

    bool testResult = testPtr();

    if (testResult == true) {
        std::cout << "\t" << colorGreen << "[PASS]"  << colorNormal << std::endl;
        ggSuccessfulTests++;
    } else {
        std::cout << colorRed << "\t[FAIL]" << colorNormal << std::endl;
    }

    ggNumTests++;
}

CBigNum
gGetTestModulus()
{
    static CBigNum testModulus(0);

    // TODO: should use a hard-coded RSA modulus for testing
    if (!testModulus) {
        CBigNum p, q;
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
Testb_GenRSAModulus()
{
    CBigNum result = gGetTestModulus();

    if (!result) {
        return false;
    }
    else {
        return true;
    }
}

bool
Testb_CalcParamSizes()
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
Testb_GenerateGroupParams()
{
    uint32_t pLen = 1024, qLen = 256, count;
    libzerocoin::IntegerGroupParams group;

    for (count = 0; count < 1; count++) {

        try {
            group = libzerocoin::deriveIntegerGroupParams(libzerocoin::calculateSeed(gGetTestModulus(), "test", ZEROCOIN_DEFAULT_SECURITYLEVEL, "TEST GROUP"), pLen, qLen);
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
Testb_ParamGen()
{
    bool result = true;

    try {
        timer.start();
        // Instantiating testParams runs the parameter generation code
        libzerocoin::ZerocoinParams testParams(gGetTestModulus(),ZEROCOIN_DEFAULT_SECURITYLEVEL);
        timer.stop();

        std::cout << "\tPARAMGEN ELAPSED TIME: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s" << std::endl;
    } catch (std::runtime_error e) {
        std::cout << e.what() << std::endl;
        result = false;
    }

    return result;
}

bool
Testb_Accumulator()
{
    // This test assumes a list of coins were generated during
    // the Testb_MintCoin() test.
    if (ggCoins[0] == NULL) {
        return false;
    }
    try {
        // Accumulate the coin list from first to last into one accumulator
        libzerocoin::Accumulator accOne(&gg_Params->accumulatorParams,libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accTwo(&gg_Params->accumulatorParams,libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accThree(&gg_Params->accumulatorParams,libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::Accumulator accFour(&gg_Params->accumulatorParams,libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::AccumulatorWitness wThree(gg_Params, accThree, ggCoins[0]->getPublicCoin());

        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            accOne += ggCoins[i]->getPublicCoin();
            accTwo += ggCoins[TESTS_COINS_TO_ACCUMULATE - (i+1)]->getPublicCoin();
            accThree += ggCoins[i]->getPublicCoin();
            wThree += ggCoins[i]->getPublicCoin();
            if(i != 0) {
                accFour += ggCoins[i]->getPublicCoin();
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
        if (!wThree.VerifyWitness(accThree, ggCoins[0]->getPublicCoin()) ) {
            std::cout << "Witness not valid" << std::endl;
            return false;
        }

    } catch (std::runtime_error e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    return true;
}

bool
Testb_MintCoin()
{
    try {
        // Generate a list of coins
        timer.start();
        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            ggCoins[i] = new libzerocoin::PrivateCoin(gg_Params, libzerocoin::CoinDenomination::ZQ_ONE);
        }
        timer.stop();
    } catch (std::exception &e) {
        return false;
    }

    std::cout << "\tMINT ELAPSED TIME:\n\t\tTotal: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s\n\t\tPer Coin: " << timer.duration()/TESTS_COINS_TO_ACCUMULATE << " ms\t" << (timer.duration()/TESTS_COINS_TO_ACCUMULATE)*0.001 << " s" << std::endl;

    return true;
}

bool
Testb_MintAndSpend()
{
    try {
        // This test assumes a list of coins were generated in Testb_MintCoin()
        if (ggCoins[0] == NULL)
        {
            // No coins: mint some.
            Testb_MintCoin();
            if (ggCoins[0] == NULL) {
                return false;
            }
        }

        // Accumulate the list of generated coins into a fresh accumulator.
        // The first one gets marked as accumulated for a witness, the
        // others just get accumulated normally.
        libzerocoin::Accumulator acc(&gg_Params->accumulatorParams, libzerocoin::CoinDenomination::ZQ_ONE);
        libzerocoin::AccumulatorWitness wAcc(gg_Params, acc, ggCoins[0]->getPublicCoin());

        timer.start();
        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            acc += ggCoins[i]->getPublicCoin();
        }
        timer.stop();

        std::cout << "\tACCUMULATOR ELAPSED TIME:\n\t\tTotal: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s\n\t\tPer Element: " << timer.duration()/TESTS_COINS_TO_ACCUMULATE << " ms\t" << (timer.duration()/TESTS_COINS_TO_ACCUMULATE)*0.001 << " s" << std::endl;

        timer.start();
        for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
            wAcc +=ggCoins[i]->getPublicCoin();
        }
        timer.stop();

        std::cout << "\tWITNESS ELAPSED TIME: \n\t\tTotal: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s\n\t\tPer Element: " << timer.duration()/TESTS_COINS_TO_ACCUMULATE << " ms\t" << (timer.duration()/TESTS_COINS_TO_ACCUMULATE)*0.001 << " s" << std::endl;

        // Now spend the coin
        timer.start();
        libzerocoin::CoinSpend spend(gg_Params, gg_Params, *(ggCoins[0]), acc, 0, wAcc, 0, libzerocoin::SpendType::SPEND); //(0) presstab
        timer.stop();

        std::cout << "\tSPEND ELAPSED TIME: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s" << std::endl;

        // Serialize the proof and deserialize into newSpend
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);

        timer.start();
        ss << spend;
        timer.stop();

        libzerocoin::CoinSpend newSpend(gg_Params, gg_Params, ss);

        std::cout << "\tSERIALIZE ELAPSED TIME: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s" << std::endl;

        // Finally, see if we can verify the deserialized proof (return our result)
        timer.start();
        bool ret = newSpend.Verify(acc);
        timer.stop();

        std::cout << "\tSPEND VERIFY ELAPSED TIME: " << timer.duration() << " ms\t" << timer.duration()*0.001 << " s" << std::endl;

        return ret;
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    return false;
}

void
Testb_RunAllTests()
{
    // Make a new set of parameters from a random RSA modulus
    gg_Params = new libzerocoin::ZerocoinParams(gGetTestModulus());

    ggNumTests = ggSuccessfulTests = 0;
    for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        ggCoins[i] = NULL;
    }

    // Run through all of the Zerocoin tests
    gLogTestResult("an RSA modulus can be generated", Testb_GenRSAModulus);
    gLogTestResult("parameter sizes are correct", Testb_CalcParamSizes);
    gLogTestResult("group/field parameters can be generated", Testb_GenerateGroupParams);
    gLogTestResult("parameter generation is correct", Testb_ParamGen);
    gLogTestResult("coins can be minted", Testb_MintCoin);
    gLogTestResult("the accumulator works", Testb_Accumulator);
    gLogTestResult("a minted coin can be spent", Testb_MintAndSpend);

    // Summarize test results
    if (ggSuccessfulTests < ggNumTests) {
        std::cout << std::endl << "ERROR: SOME TESTS FAILED" << std::endl;
    }

    // Clear any generated coins
    for (uint32_t i = 0; i < TESTS_COINS_TO_ACCUMULATE; i++) {
        delete ggCoins[i];
    }

    std::cout << ggSuccessfulTests << " out of " << ggNumTests << " tests passed." << std::endl << std::endl;
    delete gg_Params;
}

BOOST_FIXTURE_TEST_SUITE(benchmark_zerocoin, TestingSetup)

BOOST_AUTO_TEST_CASE(benchmark_test)
{
    std::cout << "libzerocoin v" << ZEROCOIN_VERSION_STRING << " benchmark utility." << std::endl << std::endl;

    Testb_RunAllTests();
}
BOOST_AUTO_TEST_SUITE_END()

