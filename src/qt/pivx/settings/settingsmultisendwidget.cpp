#include "qt/pivx/settings/settingsmultisendwidget.h"
#include "qt/pivx/settings/forms/ui_settingsmultisendwidget.h"
#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/qtutils.h"
#include "addresstablemodel.h"
#include "base58.h"
#include "init.h"
#include "walletmodel.h"

SettingsMultisendWidget::SettingsMultisendWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsMultisendWidget),
    window(_window)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setText("Multisend");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->labelSubtitle1->setText(tr("MultiSend allows you to automatically send up to 100% of your stake or masternode reward to a list of other PIVX addresses after it matures.\n\nTo Add: enter percentage to give and PIVX address to add to the MultiSend vector.\n\nTo Delete: Enter address to delete and press delete."));
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // list
    ui->listView->setVisible(false);

    //Button Group
    ui->pushLeft->setText(tr("Active"));
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText(tr("Disable"));
    ui->pushRight->setProperty("cssClass", "btn-check-right");
    ui->pushLeft->setChecked(true);

    ui->pushImgEmpty->setProperty("cssClass", "img-empty-multisend");
    ui->labelEmpty->setText(tr("No active recipient yet"));
    ui->labelEmpty->setProperty("cssClass", "text-empty");

    // Buttons
    ui->pushButtonSave->setText(tr("ADD RECIPIENT"));
    ui->pushButtonClear->setText(tr("CLEAR ALL"));
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonClear);

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onAddRecipientClicked()));
}

void SettingsMultisendWidget::onAddRecipientClicked() {
    window->showHide(true);
    SettingsMultisendDialog* dialog = new SettingsMultisendDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
}

SettingsMultisendWidget::~SettingsMultisendWidget()
{
    delete ui;
}
