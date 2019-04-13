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

    bool getAddress(WalletModel *model, QString *retAddress);
    bool selected = false;

private:
    Ui::SendChangeAddressDialog *ui;
};

#endif // SENDCHANGEADDRESSDIALOG_H
