<<<<<<< HEAD
// Copyright (c) 2022 The PIVX Core developers
=======
// Copyright (c) 2022 The hemis Core developers
>>>>>>> 1f345019d (first step)
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"
<<<<<<< HEAD
#include "test/test_pivx.h"
=======
#include "test/test_hemis.h"
>>>>>>> 1f345019d (first step)
#include "tiertwo/netfulfilledman.h"
#include "utiltime.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(netfulfilledman_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(netfulfilledman_simple_add_and_expire)
{
    int64_t now = GetTime();
    SetMockTime(now);

    CNetFulfilledRequestManager fulfilledMan(DEFAULT_ITEMS_FILTER_SIZE);
    CService service = LookupNumeric("1.1.1.1", 9999);
    std::string request = "request";
    BOOST_ASSERT(!fulfilledMan.HasFulfilledRequest(service, request));

    // Add request
    fulfilledMan.AddFulfilledRequest(service, request);
    // Verify that the request is there
    BOOST_ASSERT(fulfilledMan.HasFulfilledRequest(service, request));

    // Advance mock time to surpass FulfilledRequestExpireTime
    SetMockTime(GetMockTime() + 60 * 60 + 1);

    // Verify that the request exists and expired now
    BOOST_CHECK(fulfilledMan.Size() == 1);
    BOOST_CHECK(!fulfilledMan.HasFulfilledRequest(service, request));

    // Verify request removal
    fulfilledMan.CheckAndRemove();
    BOOST_CHECK(fulfilledMan.Size() == 0);

    // Items filter, insertion and lookup.
    uint256 item(g_insecure_rand_ctx.rand256());
    fulfilledMan.AddItemRequest(service, item);
    BOOST_CHECK(fulfilledMan.HasItemRequest(service, item));

    CService service2 = LookupNumeric("1.1.1.2", 9999);
    BOOST_CHECK(!fulfilledMan.HasItemRequest(service2, item));

    // Advance mock time to surpass the expiration time
    SetMockTime(GetMockTime() + DEFAULT_ITEMS_FILTER_CLEANUP + 1);
    fulfilledMan.CheckAndRemove();
    BOOST_CHECK(!fulfilledMan.HasItemRequest(service, item));

    // Now test filling up the filter
    fulfilledMan.AddItemRequest(service, item);
    for (int i = 0; i < 300; i++) {
        uint256 element(g_insecure_rand_ctx.rand256());
        fulfilledMan.AddItemRequest(service, element);
        BOOST_CHECK(fulfilledMan.HasItemRequest(service, element));
    }

    fulfilledMan.CheckAndRemove();
    BOOST_CHECK(!fulfilledMan.HasItemRequest(service, item));
}

BOOST_AUTO_TEST_SUITE_END()
