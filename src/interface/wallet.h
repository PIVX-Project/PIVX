// Copyright (c) 2018-2020 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_INTERFACES_WALLET_H
#define PIVX_INTERFACES_WALLET_H

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
    CAmount delegate_balance{0};
    CAmount coldstaked_balance{0};
    CAmount shield_balance{0};
    CAmount unconfirmed_shield_balance{0};

    bool balanceChanged(const WalletBalances& prev) const
    {
        return balance != prev.balance || unconfirmed_balance != prev.unconfirmed_balance ||
               immature_balance != prev.immature_balance || watch_only_balance != prev.watch_only_balance ||
               unconfirmed_watch_only_balance != prev.unconfirmed_watch_only_balance ||
               immature_watch_only_balance != prev.immature_watch_only_balance ||
               delegate_balance != prev.delegate_balance || coldstaked_balance != prev.coldstaked_balance ||
               shield_balance != prev.shield_balance || unconfirmed_shield_balance != prev.unconfirmed_shield_balance;
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
