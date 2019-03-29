#include "qt/pivx/settings/settingswalletrepairwidget.h"
#include "qt/pivx/settings/forms/ui_settingswalletrepairwidget.h"

SettingsWalletRepairWidget::SettingsWalletRepairWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWalletRepairWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    ui->scrollStack->setProperty("cssClass", "container");


    // Title

    ui->labelTitle->setText("Wallet Repair");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("The buttons below will restart the wallet with command-line options to repair this wallet, fix issues with corrupt blockchain files or missing/obsolete transactions.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelLocation->setText("Wallet in use: /Users/johndoe/Library/Application Support/PIVX/wallet.dat");
    ui->labelLocation->setProperty("cssClass", "text-main-purple");

    // Labels

    ui->labelSalvage->setText("-salvagewallet:");
    ui->labelSalvage->setProperty("cssClass", "text-subtitle");
    ui->labelMessageSalvage->setText("Attempt to recover private keys from a corrupt wallet.dat.");
    ui->labelMessageSalvage->setProperty("cssClass", "text-main-grey");

    ui->labelRescan->setText("-salvagewallet:");
    ui->labelRescan->setProperty("cssClass", "text-subtitle");
    ui->labelMessageRescan->setText("Rescan the blockchain for missing wallet transactions.");
    ui->labelMessageRescan->setProperty("cssClass", "text-main-grey");

    ui->labelRecover1->setText("-salvagewallet:");
    ui->labelRecover1->setProperty("cssClass", "text-subtitle");
    ui->labelMessageRecover1->setText("Recover transactions from blockchain (keep-meta-data, e.g. account owner).");
    ui->labelMessageRecover1->setProperty("cssClass", "text-main-grey");

    ui->labelRecover2->setText("-salvagewallet:");
    ui->labelRecover2->setProperty("cssClass", "text-subtitle");
    ui->labelMessageRecover2->setText("Recover transactions from blockchain (drop meta-data).");
    ui->labelMessageRecover2->setProperty("cssClass", "text-main-grey");

    ui->labelUpgrade->setText("-salvagewallet:");
    ui->labelUpgrade->setProperty("cssClass", "text-subtitle");
    ui->labelMessageUpgrade->setText("Upgrade wallet to latest format on startup. (Note: this is NOT an update of the wallet itself)");
    ui->labelMessageUpgrade->setProperty("cssClass", "text-main-grey");

    ui->labelRebuild->setText("-salvagewallet:");
    ui->labelRebuild->setProperty("cssClass", "text-subtitle");
    ui->labelMessageRebuild->setText("Rebuild blockchain index from current blk000???.dat files.");
    ui->labelMessageRebuild->setProperty("cssClass", "text-main-grey");

    ui->labelDelete->setText("-salvagewallet:");
    ui->labelDelete->setProperty("cssClass", "text-subtitle");
    ui->labelMessageDelete->setText("Deletes all local blockchain folders so the wallet synchronizes from scratch.");
    ui->labelMessageDelete->setProperty("cssClass", "text-main-grey");

    // Buttons

    ui->pushButtonSalvage->setText("Salvage wallet");
    ui->pushButtonSalvage->setProperty("cssClass", "btn-primary");

    ui->pushButtonRescan->setText("Rescan blockchain file");
    ui->pushButtonRescan->setProperty("cssClass", "btn-primary");

    ui->pushButtonRecover1->setText("Recover transactions 1");
    ui->pushButtonRecover1->setProperty("cssClass", "btn-primary");

    ui->pushButtonRecover2->setText("Recover transactions 2");
    ui->pushButtonRecover2->setProperty("cssClass", "btn-primary");

    ui->pushButtonUpgrade->setText("Upgrade wallet format");
    ui->pushButtonUpgrade->setProperty("cssClass", "btn-primary");

    ui->pushButtonRebuild->setText("Rebuild index");
    ui->pushButtonRebuild->setProperty("cssClass", "btn-primary");

    ui->pushButtonDelete->setText("Delete local blockchain ");
    ui->pushButtonDelete->setProperty("cssClass", "btn-primary");

}

SettingsWalletRepairWidget::~SettingsWalletRepairWidget()
{
    delete ui;
}
