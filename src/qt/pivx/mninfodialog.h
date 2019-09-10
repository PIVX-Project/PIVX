// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNINFODIALOG_H
#define MNINFODIALOG_H

#include <QDialog>
#include "qt/pivx/snackbar.h"

class WalletModel;

namespace Ui {
class MnInfoDialog;
}

class MnInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MnInfoDialog(QWidget *parent = nullptr);
    ~MnInfoDialog();

    bool exportMN = false;

    void setData(QString privKey, QString name, QString address, QString txId, QString outputIndex, QString status);

public slots:
    void closeDialog();

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
