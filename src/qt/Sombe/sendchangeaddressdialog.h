//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCHANGEADDRESSDIALOG_H
#define SENDCHANGEADDRESSDIALOG_H

#include "qt/Sombe/focuseddialog.h"
#include "qt/Sombe/snackbar.h"

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
    QString getAddress() const;

    void showEvent(QShowEvent* event) override;

private:
    WalletModel* walletModel;
    Ui::SendChangeAddressDialog *ui;
    SnackBar *snackBar = nullptr;
    void inform(const QString& text);

private Q_SLOTS:
    void reset();
    void accept() override;
};

#endif // SENDCHANGEADDRESSDIALOG_H
