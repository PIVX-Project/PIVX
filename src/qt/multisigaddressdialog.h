// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGADDRESSDIALOG_H
#define BITCOIN_QT_MULTISIGADDRESSDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui
{
class MultiSigAddressDialog;
}

class MultiSigAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiSigAddressDialog(QWidget* parent);
    ~MultiSigAddressDialog();

    void setModel(WalletModel* model);
    void setAddress(const QString& address);

    void showPage_1(bool fShow);
    void showPage_2(bool fShow);

protected:
    bool eventFilter(QObject* object, QEvent* event);

private:
    Ui::MultiSigAddressDialog* ui;
    WalletModel* model;

private slots:
   void on_continueButton_clicked();
   void on_addressBookButton_clicked();
   void on_pasteButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGADDRESSDIALOG_H
