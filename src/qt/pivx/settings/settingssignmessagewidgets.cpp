#include "qt/pivx/settings/settingssignmessagewidgets.h"
#include "qt/pivx/settings/forms/ui_settingssignmessagewidgets.h"
#include "qt/pivx/qtutils.h"

SettingsSignMessageWidgets::SettingsSignMessageWidgets(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsSignMessageWidgets)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Sign Message");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Address

    ui->labelSubtitleAddress->setText(tr("Enter a PIVX address or contact label"));
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title");

    ui->lineEditAddress->setPlaceholderText(tr("Add address"));
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditAddress);

    // Message

    ui->labelSubtitleMessage->setText(tr("Message"));
    ui->labelSubtitleMessage->setProperty("cssClass", "text-title");

    ui->lineEditMessage->setPlaceholderText("Write a message");
    initCssEditLine(ui->lineEditMessage);

    // Buttons

    ui->pushButtonSave->setText("SIGN");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

}

SettingsSignMessageWidgets::~SettingsSignMessageWidgets()
{
    delete ui;
}
