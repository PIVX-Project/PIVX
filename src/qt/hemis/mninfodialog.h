// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNINFODIALOG_H
#define MNINFODIALOG_H

#include "qt/pivx/focuseddialog.h"
#include "qt/pivx/snackbar.h"

class WalletModel;

namespace Ui {
class MnInfoDialog;
}

class MnInfoDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit MnInfoDialog(QWidget *parent = nullptr);
    ~MnInfoDialog();

    bool exportMN = false;

    void setData(const QString& _pubKey, const QString& name, const QString& address, const QString& _txId, const QString& outputIndex, const QString& status);

public Q_SLOTS:
    void reject() override;

private:
    Ui::MnInfoDialog *ui;
    SnackBar *snackBar = nullptr;
    int nDisplayUnit = 0;
    WalletModel *model = nullptr;
    QString txId;
    QString pubKey;

    void copyInform(const QString& copyStr, const QString& message);
};

#endif // MNINFODIALOG_H
