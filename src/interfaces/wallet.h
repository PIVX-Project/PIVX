// Copyright (c) 2018-2020 The Bitcoin Core developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_INTERFACES_WALLET_H
#define hemis_INTERFACES_WALLET_H

#include <amount.h>
#include "wallet/wallet.h"

namespace interfaces {

//! Collection of wallet balances.
struct WalletBalances
{
    CAmount balance{0};
    CAmount unconfirmed_balance{0};
    CAmount immature_balance{0};
    bool have_watch_only{false};
    CAmount watch_only_balance{0};
    CAmount unconfirmed_watch_only_balance{0};
    CAmount immature_watch_only_balance{0};
    bool have_coldstaking{false};
    CAmount delegate_balance{0};
    CAmount coldstaked_balance{0};
    CAmount shielded_balance{0};
    CAmount unconfirmed_shielded_balance{0};

    bool balanceChanged(const WalletBalances& prev) const
    {
        return balance != prev.balance || unconfirmed_balance != prev.unconfirmed_balance ||
               immature_balance != prev.immature_balance || watch_only_balance != prev.watch_only_balance ||
               unconfirmed_watch_only_balance != prev.unconfirmed_watch_only_balance ||
               immature_watch_only_balance != prev.immature_watch_only_balance ||
               delegate_balance != prev.delegate_balance || coldstaked_balance != prev.coldstaked_balance ||
               shielded_balance != prev.shielded_balance || unconfirmed_shielded_balance != prev.unconfirmed_shielded_balance;
    }
};

class Wallet {
public:
    explicit Wallet(CWallet& wallet) : m_wallet(wallet) { };
    // Retrieve all the wallet balances
    WalletBalances getBalances();

private:
    CWallet& m_wallet;
};


} // namespace interfaces

#endif // BITCOIN_INTERFACES_WALLET_H
