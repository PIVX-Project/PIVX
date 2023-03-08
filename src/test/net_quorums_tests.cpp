// Copyright (c) 2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include "evo/deterministicmns.h"
#include "llmq/quorums_connections.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(net_quorums_tests)

std::vector<CDeterministicMNCPtr> createMNList(unsigned int size)
{
    std::vector<CDeterministicMNCPtr> mns;
    for (size_t i = 0; i < size; i++) {
        CDeterministicMN dmn(i);
        uint256 newProTxHash;
        do {
            newProTxHash = g_insecure_rand_ctx.rand256();
        } while (std::find_if(mns.begin(), mns.end(),
                [&newProTxHash](CDeterministicMNCPtr mn){ return mn->proTxHash == newProTxHash; }) != mns.end());
        dmn.proTxHash = newProTxHash;
        mns.emplace_back(std::make_shared<const CDeterministicMN>(dmn));
    }
    return mns;
}

void checkQuorumRelayMembers(const std::vector<CDeterministicMNCPtr>& list, unsigned int expectedResSize)
{
    for (size_t i = 0; i < list.size(); i++) {
        const auto& set = llmq::GetQuorumRelayMembers(list, i);
        BOOST_CHECK_MESSAGE(set.size() == expectedResSize,
                            strprintf("size %d, expected ret size %d, ret size %d ", list.size(), expectedResSize, set.size()));
        BOOST_CHECK(set.count(list[i]->proTxHash) == 0);
    }
}

BOOST_FIXTURE_TEST_CASE(get_quorum_relay_members, BasicTestingSetup)
{
    size_t list_size = 2000;    // n
    size_t relay_memb = 10;     // floor(log2(n-1))

    std::vector<CDeterministicMNCPtr> masternodes = createMNList(list_size);

    // Test quorum sizes 2000 to 2
    while (true) {
        checkQuorumRelayMembers(masternodes, relay_memb);

        masternodes.resize(--list_size);
        if (list_size == 1) break;
        // n=2 is a special case (1 relay member)
        // Otherwise relay members are 1 + max(1, floor(log2(n-1))-1)
        else if (list_size == 2 ||
                (list_size > 4 && (1 << relay_memb) >= (int)list_size)) relay_memb--;
    }
}

BOOST_AUTO_TEST_SUITE_END()
