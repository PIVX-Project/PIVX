// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/util/blocksutil.h"
#include "validation.h"
#include "util/blockstatecatcher.h"

#include <boost/test/unit_test.hpp>

void ProcessBlockAndCheckRejectionReason(std::shared_ptr<CBlock>& pblock,
                                         const std::string& blockRejectionReason,
                                         int expectedChainHeight)
{
    BlockStateCatcher stateChecker(pblock->GetHash());
    RegisterValidationInterface(&stateChecker);
    ProcessNewBlock(pblock, nullptr);
    UnregisterValidationInterface(&stateChecker);
    BOOST_CHECK(stateChecker.found);
    CValidationState state = stateChecker.state;
    BOOST_CHECK_EQUAL(WITH_LOCK(cs_main, return chainActive.Height();), expectedChainHeight);
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), blockRejectionReason);
}
