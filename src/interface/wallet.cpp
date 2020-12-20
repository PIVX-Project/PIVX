// Copyright (c) 2018-2020 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "interface/wallet.h"
#include "wallet.h"

namespace interfaces {

    WalletBalances Wallet::getBalances() {
        WalletBalances result;
        result.balance = m_wallet.GetAvailableBalance();
        result.unconfirmed_balance = m_wallet.GetUnconfirmedBalance();
        result.immature_balance = m_wallet.GetImmatureBalance();
        result.have_watch_only = m_wallet.HaveWatchOnly();
        if (result.have_watch_only) {
            result.watch_only_balance = m_wallet.GetWatchOnlyBalance();
            result.unconfirmed_watch_only_balance = m_wallet.GetUnconfirmedWatchOnlyBalance();
            result.immature_watch_only_balance = m_wallet.GetImmatureWatchOnlyBalance();
        }
        result.delegate_balance = m_wallet.GetDelegatedBalance();
        result.coldstaked_balance = m_wallet.GetColdStakingBalance();
        result.shield_balance = m_wallet.GetAvailableShieldBalance();
        result.unconfirmed_shield_balance = m_wallet.GetUnconfirmedShieldBalance();
        return result;
    }

} // namespace interfaces