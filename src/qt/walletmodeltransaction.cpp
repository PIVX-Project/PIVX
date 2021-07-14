// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodeltransaction.h"

#include "wallet/wallet.h"

WalletModelTransaction::WalletModelTransaction(const QList<SendCoinsRecipient>& recipients) : recipients(recipients),
                                                                                              walletTransaction(0),
                                                                                              keyChange(0),
                                                                                              fee(0)
{ }

WalletModelTransaction::~WalletModelTransaction()
{
    delete keyChange;
}

QList<SendCoinsRecipient> WalletModelTransaction::getRecipients()
{
    return recipients;
}

CTransactionRef& WalletModelTransaction::getTransaction()
{
    return walletTransaction;
}

unsigned int WalletModelTransaction::getTransactionSize()
{
    return (!walletTransaction ? 0 : (::GetSerializeSize(*walletTransaction, PROTOCOL_VERSION)));
}

CAmount WalletModelTransaction::getTransactionFee()
{
    return fee;
}

void WalletModelTransaction::setTransactionFee(const CAmount& newFee)
{
    fee = newFee;
}

unsigned int WalletModelTransaction::subtractFeeFromRecipents() const
{
    unsigned int count = 0;
    for (const SendCoinsRecipient& rcp : recipients) {
        if (rcp.fSubtractFee) count++;
    }
    return count;
}

CAmount WalletModelTransaction::getTotalTransactionAmount()
{
    CAmount totalTransactionAmount = 0;
    for (const SendCoinsRecipient& rcp : recipients) {
        totalTransactionAmount += rcp.amount;
    }
    return totalTransactionAmount;
}

CReserveKey* WalletModelTransaction::newPossibleKeyChange(CWallet* wallet)
{
    keyChange = new CReserveKey(wallet);
    return keyChange;
}

CReserveKey* WalletModelTransaction::getPossibleKeyChange()
{
    return keyChange;
}
