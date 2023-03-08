// Copyright (c) 2019-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef REQUESTDIALOG_H
#define REQUESTDIALOG_H

#include "qt/pivx/focuseddialog.h"
#include "qt/pivx/snackbar.h"
#include "walletmodel.h"

#include <QPixmap>

class WalletModel;
class PIVXGUI;

namespace Ui {
class RequestDialog;
}

class RequestDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit RequestDialog(QWidget *parent = nullptr);
    ~RequestDialog();

    void setWalletModel(WalletModel *model);
    void setPaymentRequest(bool _isPaymentRequest);
    void showEvent(QShowEvent *event) override;
    int res = -1;

private Q_SLOTS:
    void accept() override;
    void onCopyClicked();
    void onCopyUriClicked();

private:
    Ui::RequestDialog *ui{nullptr};
    int pos = 0;
    bool isPaymentRequest = true;
    WalletModel *walletModel{nullptr};
    SnackBar *snackBar{nullptr};
    // Cached last address
    SendCoinsRecipient *info{nullptr};

    void updateQr(const QString& str);
    void inform(const QString& text);
};

#endif // REQUESTDIALOG_H
