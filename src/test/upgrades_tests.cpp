// Copyright (c) 2017 The Zcash Core developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/upgrades.h"
#include "optional.h"
#include "test/test_hemis.h"

#include <boost/test/unit_test.hpp>

struct UpgradesTest : public TestingSetup
{
    int DefaultActivation[Consensus::MAX_NETWORK_UPGRADES];
    UpgradesTest()
    {
        SelectParams(CBaseChainParams::REGTEST);
        // Save activation heights in DefaultActivation and set all upgrades to inactive.
        for (uint32_t idx = Consensus::BASE_NETWORK + 1; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
            DefaultActivation[idx] = Params().GetConsensus().vUpgrades[idx].nActivationHeight;
            UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex(idx), Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
        }
    }
    ~UpgradesTest()
    {
        // Revert to default
        for (uint32_t idx = Consensus::BASE_NETWORK + 1; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
            UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex(idx), DefaultActivation[idx]);
        }
        SelectParams(CBaseChainParams::MAIN);
    }
};

BOOST_FIXTURE_TEST_SUITE(network_upgrades_tests, UpgradesTest)

BOOST_AUTO_TEST_CASE(networkUpgradeStateTest)
{
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_DISABLED);
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_DISABLED);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_ACTIVE);
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_ACTIVE);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(0, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_PENDING);
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(nActivationHeight - 1, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_PENDING);
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(nActivationHeight, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_ACTIVE);
    BOOST_CHECK_EQUAL(
            NetworkUpgradeState(1000000, params, Consensus::UPGRADE_TESTDUMMY),
            UPGRADE_ACTIVE);
}

BOOST_AUTO_TEST_CASE(IsActivationHeightTest)
{
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    BOOST_CHECK(!IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    BOOST_CHECK(!IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    BOOST_CHECK(!IsActivationHeight(-1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(0, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(nActivationHeight - 1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(IsActivationHeight(nActivationHeight, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(nActivationHeight + 1, params, Consensus::UPGRADE_TESTDUMMY));
    BOOST_CHECK(!IsActivationHeight(1000000, params, Consensus::UPGRADE_TESTDUMMY));
}

BOOST_AUTO_TEST_CASE(IsActivationHeightForAnyUpgradeTest)
{
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(-1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(0, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1000000, params));

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(-1, params));
    BOOST_CHECK(IsActivationHeightForAnyUpgrade(0, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1000000, params));

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(-1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(0, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(nActivationHeight - 1, params));
    BOOST_CHECK(IsActivationHeightForAnyUpgrade(nActivationHeight, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(nActivationHeight + 1, params));
    BOOST_CHECK(!IsActivationHeightForAnyUpgrade(1000000, params));
}

BOOST_AUTO_TEST_CASE(NextActivationHeightTest)
{
    const Consensus::Params& params = Params().GetConsensus();

    // Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT
    BOOST_CHECK(NextActivationHeight(-1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(0, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(1000000, params) == nullopt);

    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    BOOST_CHECK(NextActivationHeight(-1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(0, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(1000000, params) == nullopt);

    int nActivationHeight = 100;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_TESTDUMMY, nActivationHeight);

    BOOST_CHECK(NextActivationHeight(-1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(0, params) == nActivationHeight);
    BOOST_CHECK(NextActivationHeight(1, params) == nActivationHeight);
    BOOST_CHECK(NextActivationHeight(nActivationHeight - 1, params) == nActivationHeight);
    BOOST_CHECK(NextActivationHeight(nActivationHeight, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(nActivationHeight + 1, params) == nullopt);
    BOOST_CHECK(NextActivationHeight(1000000, params) == nullopt);
}

BOOST_AUTO_TEST_SUITE_END()
