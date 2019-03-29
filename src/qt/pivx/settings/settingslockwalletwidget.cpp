#include "qt/pivx/settings/settingslockwalletwidget.h"
#include "qt/pivx/settings/forms/ui_settingslockwalletwidget.h"

SettingsLockWalletWidget::SettingsLockWalletWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsLockWalletWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Lock and Unlocked Wallet ");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");


    // Radio buttons

    ui->radioButtonMap->setText("Wallet unlocked");

    ui->radioButtonAllow->setText("Wallet locked");
    ui->radioButtonConnect->setText("Locked for staking");

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

}

SettingsLockWalletWidget::~SettingsLockWalletWidget()
{
    delete ui;
}
