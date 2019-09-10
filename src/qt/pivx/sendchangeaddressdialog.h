// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCHANGEADDRESSDIALOG_H
#define SENDCHANGEADDRESSDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class SendChangeAddressDialog;
}

class SendChangeAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendChangeAddressDialog(QWidget *parent = nullptr);
    ~SendChangeAddressDialog();

    void setAddress(QString address);
    bool getAddress(WalletModel *model, QString *retAddress);
    bool selected = false;

private:
    Ui::SendChangeAddressDialog *ui;
};

#endif // SENDCHANGEADDRESSDIALOG_H
