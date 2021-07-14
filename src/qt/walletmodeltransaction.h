// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETMODELTRANSACTION_H
#define BITCOIN_QT_WALLETMODELTRANSACTION_H

#include "walletmodel.h"

#include "amount.h"
#include "primitives/transaction.h"

#include <QObject>

class SendCoinsRecipient;

class CReserveKey;
class CWallet;
class CWalletTx;

/** Data model for a walletmodel transaction. */
class WalletModelTransaction
{
public:
    explicit WalletModelTransaction(const QList<SendCoinsRecipient>& recipients);
    ~WalletModelTransaction();

    QList<SendCoinsRecipient> getRecipients();

    unsigned int getTransactionSize();

    void setTransactionFee(const CAmount& newFee);
    CAmount getTransactionFee();

    CAmount getTotalTransactionAmount();

    CReserveKey* newPossibleKeyChange(CWallet* wallet);
    CReserveKey* getPossibleKeyChange();

    CTransactionRef& getTransaction();

    // return the number of recipients with subtract-fee-from-amount
    unsigned int subtractFeeFromRecipents() const;

    // Whether should create a +v2 tx or go simple and create a v1.
    bool useV2{false};
    bool fIsStakeDelegationVoided{false};

private:
    const QList<SendCoinsRecipient> recipients;
    CTransactionRef walletTransaction;
    CReserveKey* keyChange{nullptr};
    CAmount fee;
};

#endif // BITCOIN_QT_WALLETMODELTRANSACTION_H
