// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"
#include "consensus/merkle.h"
#include "wallet/wallet.h"

#include "blockassembler.h"
#include "blocksignature.h"
#include "sapling/address.h"

#include <boost/test/unit_test.hpp>

TestPoSChainSetup::TestPoSChainSetup() : TestChainSetup(0)
{
    initZKSNARKS(); // init zk-snarks lib

    bool fFirstRun;
    pwalletMain = std::make_unique<CWallet>("testWallet", WalletDatabase::CreateMock());
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain.get());

    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        gArgs.ForceSetArg("-keypool", "5");
        pwalletMain->SetupSPKM(true);

        // import coinbase key used to generate the 100-blocks chain
        BOOST_CHECK(pwalletMain->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey()));
    }

    int posActivation = Params().GetConsensus().vUpgrades[Consensus::UPGRADE_POS].nActivationHeight - 1;
    for (int i = 0; i < posActivation; i++) {
        CBlock b = CreateAndProcessBlock({}, coinbaseKey);
        coinbaseTxns.emplace_back(*b.vtx[0]);
    }
}

TestPoSChainSetup::~TestPoSChainSetup()
{
    SyncWithValidationInterfaceQueue();
    UnregisterValidationInterface(pwalletMain.get());
}

// Util functions that are useful in all unit tests that use this POS chain
// Create a sapling operation that can build a shield tx
SaplingOperation CreateOperationAndBuildTx(std::unique_ptr<CWallet>& pwallet,
    CAmount amount,
    bool selectTransparentCoins)
{
    // Create the operation
    libzcash::SaplingPaymentAddress pa = pwallet->GenerateNewSaplingZKey("s1");
    std::vector<SendManyRecipient> recipients;
    recipients.emplace_back(pa, amount, "", false);
    SaplingOperation operation(Params().GetConsensus(), pwallet.get());
    operation.setMinDepth(1);
    auto operationResult = operation.setRecipients(recipients)
                               ->setSelectTransparentCoins(selectTransparentCoins)
                               ->setSelectShieldedCoins(!selectTransparentCoins)
                               ->build();
    BOOST_ASSERT_MSG(operationResult, operationResult.getError().c_str());

    CValidationState state;
    BOOST_ASSERT_MSG(
        CheckTransaction(operation.getFinalTx(), state, true),
        "Invalid Sapling transaction");
    return operation;
}

bool IsSpentOnFork(const COutput& coin, std::initializer_list<std::shared_ptr<CBlock>> forkchain)
{
    for (const auto& block : forkchain) {
        const auto& usedOutput = block->vtx[1]->vin.at(0).prevout;
        if (coin.tx->GetHash() == usedOutput.hash && coin.i == (int)usedOutput.n) {
            // spent on fork
            return true;
        }
    }
    return false;
}

std::shared_ptr<CBlock> CreateBlockInternal(CWallet* pwalletMain, const std::vector<CMutableTransaction>& txns, CBlockIndex* customPrevBlock, std::initializer_list<std::shared_ptr<CBlock>> forkchain, bool fNoMempoolTx, bool testValidity)
{
    std::vector<CStakeableOutput> availableCoins;
    BOOST_CHECK(pwalletMain->StakeableCoins(&availableCoins));

    // Remove any utxo which is not deeper than 120 blocks (for the same reasoning
    // used when selecting tx inputs in CreateAndCommitTx)
    // Also, as the wallet is not prepared to follow several chains at the same time,
    // need to manually remove from the stakeable utxo set every already used
    // coinstake inputs on the previous blocks of the parallel chain so they
    // are not used again.
    for (auto it = availableCoins.begin(); it != availableCoins.end();) {
        if (it->nDepth <= 120 || IsSpentOnFork(*it, forkchain)) {
            it = availableCoins.erase(it);
        } else {
            it++;
        }
    }

    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(
        Params(), false)
                                                         .CreateNewBlock(CScript(),
                                                             pwalletMain,
                                                             true,
                                                             &availableCoins,
                                                             fNoMempoolTx,
                                                             testValidity,
                                                             customPrevBlock,
                                                             false);
    BOOST_ASSERT(pblocktemplate);
    auto pblock = std::make_shared<CBlock>(pblocktemplate->block);
    if (!txns.empty()) {
        for (const auto& tx : txns) {
            pblock->vtx.emplace_back(MakeTransactionRef(tx));
        }
        pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        const int nHeight = (customPrevBlock != nullptr ? customPrevBlock->nHeight + 1 : WITH_LOCK(cs_main, return chainActive.Height()) + 1);
        pblock->hashFinalSaplingRoot = CalculateSaplingTreeRoot(&*pblock, nHeight, Params());
        assert(SignBlock(*pblock, *pwalletMain));
    }
    return pblock;
}
