// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCUSTOMFEEDIALOG_H
#define SENDCUSTOMFEEDIALOG_H

#include <QDialog>
#include "amount.h"

class WalletModel;

namespace Ui {
class SendCustomFeeDialog;
}

class SendCustomFeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCustomFeeDialog(QWidget *parent = nullptr);
    ~SendCustomFeeDialog();

    void setWalletModel(WalletModel* model);
    void showEvent(QShowEvent *event) override;
    CFeeRate getFeeRate();
    void clear();

public slots:
    void onRecommendedChecked();
    void onCustomChecked();
    void updateFee();
    void onChangeTheme(bool isLightTheme, QString& theme);
private:
    Ui::SendCustomFeeDialog *ui;
    WalletModel* walletModel = nullptr;
    CFeeRate feeRate;
};

#endif // SENDCUSTOMFEEDIALOG_H
