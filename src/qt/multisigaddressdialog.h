// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGADDRESSDIALOG_H
#define BITCOIN_QT_MULTISIGADDRESSDIALOG_H

#include <QDialog>
#include <QFrame>
#include "script/script.h"

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
    void setAddress(const QString& address);

    void showPage(int pageNumber);

protected:
    bool eventFilter(QObject* object, QEvent* event);

private:
    Ui::MultisigAddressDialog* ui;
    WalletModel* model;
    CScript createRedeemScript(int m, std::vector<std::string> keys);
    QFrame* createAddress(int labelNumber);
    QFrame* createInput(int labelNumber);

private slots:
   void on_addAddressButton_ADD_clicked();
   void on_addAddressButton_SIG_clicked();
   void on_addressBookButton_clicked();
   void on_pasteButton_clicked();
   void on_addMultisigButton_clicked();
   void on_addDestinationButton_clicked();
   void on_createButton_clicked();
   void on_addInputButton_SIG_clicked();
   void on_addInputButton_CRE_clicked();
   void on_addAddressButton_clicked();
   void on_addPKButton_clicked();
   void on_signButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGADDRESSDIALOG_H
