#include "qt/pivx/settings/settingschangepasswordwidget.h"
#include "qt/pivx/settings/forms/ui_settingschangepasswordwidget.h"
#include "QGraphicsDropShadowEffect"

SettingsChangePasswordWidget::SettingsChangePasswordWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsChangePasswordWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Change passphrase");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Effects

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);


    QGraphicsDropShadowEffect* shadowEffect3 = new QGraphicsDropShadowEffect();
    shadowEffect3->setColor(QColor(0, 0, 0, 22));
    shadowEffect3->setXOffset(0);
    shadowEffect3->setYOffset(3);
    shadowEffect3->setBlurRadius(6);

    // Current Password

    ui->labelSubtitleCurrent->setText("Current passphrase");
    ui->labelSubtitleCurrent->setProperty("cssClass", "text-title");

    ui->lineEditCurrent->setPlaceholderText("Enter a current passphrase");
    ui->lineEditCurrent->setProperty("cssClass", "edit-primary");
    ui->lineEditCurrent->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditCurrent->setEchoMode(QLineEdit::Password);
    ui->lineEditCurrent->setGraphicsEffect(shadowEffect);

    // New Password

    ui->labelSubtitleNew->setText("New passphrase");
    ui->labelSubtitleNew->setProperty("cssClass", "text-title");

    ui->lineEditNew->setPlaceholderText("Enter a new passphrase");
    ui->lineEditNew->setProperty("cssClass", "edit-primary");
    ui->lineEditNew->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditNew->setEchoMode(QLineEdit::Password);
    ui->lineEditNew->setGraphicsEffect(shadowEffect2);

    // Repeat Password

    ui->labelSubtitleRepeat->setText("Repeat passphrase");
    ui->labelSubtitleRepeat->setProperty("cssClass", "text-title");

    ui->lineEditRepeat->setPlaceholderText("Repeat the new passphrase");
    ui->lineEditRepeat->setProperty("cssClass", "edit-primary");
    ui->lineEditRepeat->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditRepeat->setEchoMode(QLineEdit::Password);
    ui->lineEditRepeat->setGraphicsEffect(shadowEffect3);

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");
}

SettingsChangePasswordWidget::~SettingsChangePasswordWidget()
{
    delete ui;
}
