#include "qt/pivx/settings/settingswalletoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingswalletoptionswidget.h"
#include "QListView"

SettingsWalletOptionsWidget::SettingsWalletOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWalletOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Wallet");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Combobox

    ui->labelTitleStake->setText("Stake split threshold:");
    ui->labelTitleStake->setProperty("cssClass", "text-title");


    ui->comboBoxStake->setProperty("cssClass", "btn-combo");

    QListView * listViewStake = new QListView();
    ui->comboBoxStake->setView(listViewStake);

    ui->comboBoxStake->addItem("2000");
    ui->comboBoxStake->addItem("3000");

    // Radio buttons

    ui->labelTitleExpert->setText("Expert");
    ui->labelTitleExpert->setProperty("cssClass", "text-title");

    ui->radioButtonEnable->setText("Enable con control features");
    ui->radioButtonShow->setText("Show Masternodes tab");
    ui->radioButtonSpend->setText("Spend unconfirmed change");

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");
}

SettingsWalletOptionsWidget::~SettingsWalletOptionsWidget()
{
    delete ui;
}
