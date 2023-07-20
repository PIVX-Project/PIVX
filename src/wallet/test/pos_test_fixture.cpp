// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/pos_test_fixture.h"
#include "wallet/wallet.h"

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
