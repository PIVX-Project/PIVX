#include "qt/pivx/settings/settingsmultisendwidget.h"
#include "qt/pivx/settings/forms/ui_settingsmultisendwidget.h"
#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/qtutils.h"

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


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // list

    ui->listView->setVisible(false);

    //ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-multisend");

    ui->labelEmpty->setText("No active recipient yet");
    ui->labelEmpty->setProperty("cssClass", "text-empty");

    // Buttons

    ui->pushButtonSave->setText("ADD RECIPIENT");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonClear->setText("CLEAR ALL");
    ui->pushButtonClear->setProperty("cssClass", "btn-text-primary");

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
