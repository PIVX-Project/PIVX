#ifndef SENDCONFIRMDIALOG_H
#define SENDCONFIRMDIALOG_H

#include <QDialog>
#include "walletmodeltransaction.h"

class WalletModelTransaction;
class WalletModel;

namespace Ui {
class SendConfirmDialog;
}

class SendConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendConfirmDialog(QWidget *parent = nullptr);
    ~SendConfirmDialog();

    bool isConfirm() { return this->confirm;}
    WalletModel::SendCoinsReturn getStatus() { return this->sendStatus;}

    void setData(WalletModel *model, WalletModelTransaction &tx);
    void setDisplayUnit(int unit){this->nDisplayUnit = unit;};

public slots:
    void acceptTx();

private:
    Ui::SendConfirmDialog *ui;
    int nDisplayUnit = 0;
    bool confirm = false;
    WalletModel *model;
    WalletModel::SendCoinsReturn sendStatus;
    WalletModelTransaction *tx;
};

#endif // SENDCONFIRMDIALOG_H
