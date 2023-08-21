// Copyright (c) 2019-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMEMASTERWIZARDDIALOG_H
#define GAMEMASTERWIZARDDIALOG_H

#include "qt/hemis/focuseddialog.h"
#include "qt/hemis/snackbar.h"
#include "gamemasterconfig.h"
#include "qt/hemis/pwidget.h"

class MNModel;
class WalletModel;

namespace Ui {
class MasterNodeWizardDialog;
class QPushButton;
}

class MasterNodeWizardDialog : public FocusedDialog, public PWidget::Translator
{
    Q_OBJECT

public:
    explicit MasterNodeWizardDialog(WalletModel* walletMode,
                                    MNModel* mnModel,
                                    QWidget *parent = nullptr);
    ~MasterNodeWizardDialog() override;
    void showEvent(QShowEvent *event) override;
    QString translate(const char *msg) override { return tr(msg); }

    QString returnStr = "";
    bool isOk = false;
    CGamemasterConfig::CGamemasterEntry* mnEntry = nullptr;

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

    WalletModel* walletModel{nullptr};
    MNModel* mnModel{nullptr};
    bool createMN();
    void inform(const QString& text);
};

#endif // GAMEMASTERWIZARDDIALOG_H
