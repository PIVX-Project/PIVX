// Copyright (c) 2018-2020 The Bitcoin Core developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "interfaces/wallet.h"
#include "wallet.h"

namespace interfaces {

    WalletBalances Wallet::getBalances() {
        WalletBalances result;
        CWallet::Balance balance = m_wallet.GetBalance();
        result.balance = balance.m_mine_trusted + balance.m_mine_trusted_shield;
        result.unconfirmed_balance = balance.m_mine_untrusted_pending;
        result.immature_balance = balance.m_mine_immature;
        result.have_watch_only = m_wallet.HaveWatchOnly();
        if (result.have_watch_only) {
            result.watch_only_balance = m_wallet.GetWatchOnlyBalance();
            result.unconfirmed_watch_only_balance = m_wallet.GetUnconfirmedWatchOnlyBalance();
            result.immature_watch_only_balance = m_wallet.GetImmatureWatchOnlyBalance();
        }
        result.delegate_balance = balance.m_mine_cs_delegated_trusted;
        if (result.have_coldstaking) { // At the moment, the GUI is not using the cold staked balance.
            result.coldstaked_balance = m_wallet.GetColdStakingBalance();
        }
        result.shielded_balance = balance.m_mine_trusted_shield;
        result.unconfirmed_shielded_balance = balance.m_mine_untrusted_shielded_balance;
        return result;
    }

} // namespace interfaces
