// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_SENDCHANGEADDRESSDIALOG_H
#define PIVX_QT_SENDCHANGEADDRESSDIALOG_H

#include "destination_io.h"
#include "focuseddialog.h"
#include "script/standard.h"
#include "snackbar.h"

class WalletModel;

namespace Ui {
class SendChangeAddressDialog;
}

class SendChangeAddressDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit SendChangeAddressDialog(QWidget* parent, WalletModel* model, bool isTransparent);
    ~SendChangeAddressDialog();

    void setAddress(QString address);
    CWDestination getDestination() const;

    void showEvent(QShowEvent* event) override;

private:
    bool isTransparent;
    WalletModel* walletModel;
    Ui::SendChangeAddressDialog *ui;
    SnackBar *snackBar = nullptr;
    CWDestination dest;

    void inform(const QString& text);

private Q_SLOTS:
    void reset();
    void accept() override;
};

#endif // PIVX_QT_SENDCHANGEADDRESSDIALOG_H
