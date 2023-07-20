// Copyright (c) 2016-2021 The Bitcoin Core developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "rpc/server.h"
#include "wallet/db.h"
#include "wallet/rpcwallet.h"
#include "wallet/wallet.h"

WalletTestingSetupBase::WalletTestingSetupBase(const std::string& chainName,
                                               const std::string& wallet_name,
                                               std::unique_ptr<WalletDatabase> db) :
        SaplingTestingSetup(chainName), m_wallet(wallet_name, std::move(db))
{
    bool fFirstRun;
    m_wallet.LoadWallet(fFirstRun);
    RegisterValidationInterface(&m_wallet);

    RegisterWalletRPCCommands(tableRPC);
}

WalletTestingSetupBase::~WalletTestingSetupBase()
{
    UnregisterValidationInterface(&m_wallet);
}

WalletTestingSetup::WalletTestingSetup(const std::string& chainName) :
        WalletTestingSetupBase(chainName, "mock", WalletDatabase::CreateMock()) {}
