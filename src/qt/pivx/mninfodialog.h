// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNINFODIALOG_H
#define MNINFODIALOG_H

#include <QDialog>

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

    void setData(QString privKey, QString name, QString address, QString txId, QString outputIndex, QString status);

private:
    Ui::MnInfoDialog *ui;
    int nDisplayUnit = 0;
    WalletModel *model = nullptr;
    QString txId;
    QString pubKey;
};

#endif // MNINFODIALOG_H
