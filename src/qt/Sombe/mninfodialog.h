//Copyright (c) 2019 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNINFODIALOG_H
#define MNINFODIALOG_H

#include "qt/Sombe/focuseddialog.h"
#include "qt/Sombe/snackbar.h"

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

    void setData(QString privKey, QString name, QString address, QString txId, QString outputIndex, QString status);

public Q_SLOTS:
    void reject() override;

private:
    Ui::MnInfoDialog *ui;
    SnackBar *snackBar = nullptr;
    int nDisplayUnit = 0;
    WalletModel *model = nullptr;
    QString txId;
    QString pubKey;

    void copyInform(QString& copyStr, QString message);
};

#endif // MNINFODIALOG_H
