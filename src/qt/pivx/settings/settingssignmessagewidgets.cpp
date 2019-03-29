#include "qt/pivx/settings/settingssignmessagewidgets.h"
#include "qt/pivx/settings/forms/ui_settingssignmessagewidgets.h"
#include "QGraphicsDropShadowEffect"

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

    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22)
);
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditAddress->setPlaceholderText("Add address");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAddress->setGraphicsEffect(shadowEffect);

    // Message

    ui->labelSubtitleMessage->setText("Message");
    ui->labelSubtitleMessage->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22)
);
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    ui->lineEditMessage->setPlaceholderText("Write a message");
    ui->lineEditMessage->setProperty("cssClass", "edit-primary");
    ui->lineEditMessage->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditMessage->setGraphicsEffect(shadowEffect2);

    // Buttons

    ui->pushButtonSave->setText("SIGN");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

}

SettingsSignMessageWidgets::~SettingsSignMessageWidgets()
{
    delete ui;
}
