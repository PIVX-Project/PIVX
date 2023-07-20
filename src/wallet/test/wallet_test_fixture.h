// Copyright (c) 2016-2021 The Bitcoin Core developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_WALLET_TEST_FIXTURE_H
#define hemis_WALLET_TEST_FIXTURE_H

#include "test/librust/sapling_test_fixture.h"
#include "wallet/wallet.h"


/** Testing setup and teardown for wallet.
 */
struct WalletTestingSetupBase : public SaplingTestingSetup
{
    WalletTestingSetupBase(const std::string& chainName,
                           const std::string& wallet_name,
                           std::unique_ptr<WalletDatabase> db);
    ~WalletTestingSetupBase();
    CWallet m_wallet;
};

struct WalletTestingSetup : public WalletTestingSetupBase
{
    WalletTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
};

struct WalletRegTestingSetup : public WalletTestingSetup
{
    WalletRegTestingSetup() : WalletTestingSetup(CBaseChainParams::REGTEST) {}
};

#endif // hemis_WALLET_TEST_FIXTURE_H

