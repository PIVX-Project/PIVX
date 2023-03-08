// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCHANGEADDRESSDIALOG_H
#define SENDCHANGEADDRESSDIALOG_H

#include "script/standard.h"
#include "qt/pivx/focuseddialog.h"
#include "qt/pivx/snackbar.h"

class WalletModel;

namespace Ui {
class SendChangeAddressDialog;
}

class SendChangeAddressDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit SendChangeAddressDialog(QWidget* parent, WalletModel* model);
    ~SendChangeAddressDialog();

    void setAddress(QString address);
    CTxDestination getDestination() const;

    void showEvent(QShowEvent* event) override;

private:
    WalletModel* walletModel;
    Ui::SendChangeAddressDialog *ui;
    SnackBar *snackBar = nullptr;
    CTxDestination dest;

    void inform(const QString& text);

private Q_SLOTS:
    void reset();
    void accept() override;
};

#endif // SENDCHANGEADDRESSDIALOG_H
