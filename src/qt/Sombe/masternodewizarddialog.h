//Copyright (c) 2019 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODEWIZARDDIALOG_H
#define MASTERNODEWIZARDDIALOG_H

#include "walletmodel.h"
#include "qt/Sombe/focuseddialog.h"
#include "qt/Sombe/snackbar.h"
#include "masternodeconfig.h"
#include "qt/Sombe/pwidget.h"

class WalletModel;

namespace Ui {
class MasterNodeWizardDialog;
class QPushButton;
}

class MasterNodeWizardDialog : public FocusedDialog, public PWidget::Translator
{
    Q_OBJECT

public:
    explicit MasterNodeWizardDialog(WalletModel *walletMode, QWidget *parent = nullptr);
    ~MasterNodeWizardDialog();
    void showEvent(QShowEvent *event) override;
    QString translate(const char *msg) override { return tr(msg); }

    QString returnStr = "";
    bool isOk = false;
    CMasternodeConfig::CMasternodeEntry* mnEntry = nullptr;

private Q_SLOTS:
    void accept() override;
    void onBackClicked();
private:
    Ui::MasterNodeWizardDialog *ui;
    QPushButton* icConfirm1;
    QPushButton* icConfirm3;
    QPushButton* icConfirm4;
    SnackBar *snackBar = nullptr;
    int pos = 0;

    WalletModel *walletModel = nullptr;
    bool createMN();
    void inform(QString text);
    void initBtn(std::initializer_list<QPushButton*> args);
};

#endif // MASTERNODEWIZARDDIALOG_H
