// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef SENDMEMODIALOG_H
#define SENDMEMODIALOG_H

#include "qt/pivx/focuseddialog.h"

class WalletModel;
class SnackBar;

namespace Ui {
class SendMemoDialog;
}

class SendMemoDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit SendMemoDialog(QWidget* parent, WalletModel* model);
    ~SendMemoDialog();

    QString getMemo();
    void setMemo(QString memo);
    // false if the operation was cancelled clicking the 'X'. No need to perform any change in the caller side.
    bool getOperationResult() { return operationResult; }
    void showEvent(QShowEvent* event) override;

public Q_SLOTS:
    void textChanged();

private:
    WalletModel* walletModel;
    Ui::SendMemoDialog *ui;
    SnackBar *snackBar = nullptr;
    bool operationResult{false};

    void inform(const QString& text);

private Q_SLOTS:
    void reset();
    void accept() override;
};

#endif // SENDMEMODIALOG_H
