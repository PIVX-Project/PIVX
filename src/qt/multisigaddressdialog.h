// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGADDRESSDIALOG_H
#define BITCOIN_QT_MULTISIGADDRESSDIALOG_H

#include <QDialog>
#include <QFrame>
#include <QVBoxLayout>
#include "script/script.h"
#include "../primitives/transaction.h"
#include "../coins.h"


class WalletModel;

namespace Ui
{
class MultisigAddressDialog;
}

class MultisigAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultisigAddressDialog(QWidget* parent);
    ~MultisigAddressDialog();
    void setModel(WalletModel* model);
    void showDialog();

protected:
    bool eventFilter(QObject* object, QEvent* event);

private:
    Ui::MultisigAddressDialog* ui;
    WalletModel* model;
    bool isFirstPK;
    CCoinsViewCache accessInputCoins(std::vector<CTxIn>& vin);
    CScript createRedeemScript(int m, std::vector<std::string> keys);
    QFrame* createAddress(int labelNumber);
    QFrame* createInput(int labelNumber);
    bool signTxFromLocalWallet(CMutableTransaction& tx, std::string& errorMessageOut, QVBoxLayout* keyList = nullptr);

private slots:
   void deleteFrame();
   void addressBookButtonReceiving();
   void on_addAddressButton_clicked();
   void on_pushButton_clicked();
   void on_pasteButton_clicked();
   void on_addMultisigButton_clicked();
   void on_addDestinationButton_clicked();
   void on_createButton_clicked();
   void on_addInputButton_clicked();
   void on_addPKButton_clicked();
   void on_signButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGADDRESSDIALOG_H
