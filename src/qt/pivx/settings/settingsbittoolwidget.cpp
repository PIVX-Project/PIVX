#include "qt/pivx/settings/settingsbittoolwidget.h"
#include "qt/pivx/settings/forms/ui_settingsbittoolwidget.h"
#include "QGraphicsDropShadowEffect"

SettingsBitToolWidget::SettingsBitToolWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBitToolWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);


    /* Title */
    ui->labelTitle->setText("BIP38 Tool");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    //Button Group

    ui->pushLeft->setText("Encrypt");
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText("Decrypt");
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");


    // Key

    ui->labelSubtitleKey->setText("Encryptied key");
    ui->labelSubtitleKey->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    ui->lineEditKey->setPlaceholderText("Enter a encrypted key");
    ui->lineEditKey->setProperty("cssClass", "edit-primary");
    ui->lineEditKey->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditKey->setGraphicsEffect(shadowEffect2);

    // Passphrase

    ui->labelSubtitlePassphrase->setText("Passphrase");
    ui->labelSubtitlePassphrase->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditPassphrase->setPlaceholderText("Enter a passphrase ");
    ui->lineEditPassphrase->setProperty("cssClass", "edit-primary");
    ui->lineEditPassphrase->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditPassphrase->setGraphicsEffect(shadowEffect);


    // Buttons

    ui->pushButtonSave->setText("DENCRYPT KEY");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonImport->setText("Import Address");
    ui->pushButtonImport->setProperty("cssClass", "btn-text-primary");
}

SettingsBitToolWidget::~SettingsBitToolWidget()
{
    delete ui;
}
