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
private:
    Ui::SendCustomFeeDialog *ui;
    WalletModel* walletModel = nullptr;
    CFeeRate feeRate;
};

#endif // SENDCUSTOMFEEDIALOG_H
