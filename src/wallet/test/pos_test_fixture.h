// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_POS_TEST_FIXTURE_H
#define PIVX_POS_TEST_FIXTURE_H

#include "test/test_pivx.h"


#include "sapling/sapling_operation.h"

class CWallet;

/*
 * A text fixture with a preloaded 250-blocks regtest chain running running on PoS
 * and a wallet containing the key used for the coinbase outputs.
 */
struct TestPoSChainSetup: public TestChainSetup
{
    std::unique_ptr<CWallet> pwalletMain;

    TestPoSChainSetup();
    ~TestPoSChainSetup();
};

SaplingOperation CreateOperationAndBuildTx(std::unique_ptr<CWallet>& pwallet,
    CAmount amount,
    bool selectTransparentCoins);
bool IsSpentOnFork(const COutput& coin, std::initializer_list<std::shared_ptr<CBlock>> forkchain = {});
std::shared_ptr<CBlock> CreateBlockInternal(CWallet* pwalletMain, const std::vector<CMutableTransaction>& txns = {}, CBlockIndex* customPrevBlock = nullptr, std::initializer_list<std::shared_ptr<CBlock>> forkchain = {}, bool fNoMempoolTx = true, bool testValidity = false);

#endif // PIVX_POS_TEST_FIXTURE_H
