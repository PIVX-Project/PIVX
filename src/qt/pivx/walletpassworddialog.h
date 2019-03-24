#ifndef WALLETPASSWORDDIALOG_H
#define WALLETPASSWORDDIALOG_H

#include <QDialog>
#include <QCheckBox>

namespace Ui {
class WalletPasswordDialog;
class QCheckBox;
}

class WalletPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletPasswordDialog(QWidget *parent = nullptr);
    ~WalletPasswordDialog();

private slots:
    void onWatchClicked();
private:
    Ui::WalletPasswordDialog *ui;
    QCheckBox *btnWatch;
};

#endif // WALLETPASSWORDDIALOG_H
