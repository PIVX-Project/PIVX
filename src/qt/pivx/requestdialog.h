#ifndef REQUESTDIALOG_H
#define REQUESTDIALOG_H

#include <QDialog>
#include <QPixmap>
#include "walletmodel.h"

class WalletModel;

namespace Ui {
class RequestDialog;
}

class RequestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RequestDialog(QWidget *parent = nullptr);
    ~RequestDialog();

    void setWalletModel(WalletModel *model);

private slots:
    void onNextClicked();
    void onCopyClicked();

private:
    Ui::RequestDialog *ui;
    int pos = 0;
    WalletModel *walletModel;
    // Cached last address
    SendCoinsRecipient *info = nullptr;

    QPixmap *qrImage = nullptr;

    void updateQr(QString str);
};

#endif // REQUESTDIALOG_H
