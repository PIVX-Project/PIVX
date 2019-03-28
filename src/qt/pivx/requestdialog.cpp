#include "qt/pivx/requestdialog.h"
#include "qt/pivx/forms/ui_requestdialog.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

RequestDialog::RequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RequestDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Text

    ui->labelTitle->setText("New Request Payment");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

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

    // Combo Coins

    ui->comboBoxCoin->setProperty("cssClass", "btn-combo-coins");
    ui->comboContainer->setProperty("cssClass", "container-purple");

    QListView * listView = new QListView();

    ui->comboBoxCoin->addItem("PIV");
    ui->comboBoxCoin->addItem("zPIV");
    ui->comboBoxCoin->setView(listView);


    // Label

    ui->labelSubtitleLabel->setText("Label");
    ui->labelSubtitleLabel->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditLabel->setPlaceholderText("Enter a label to be saved withing the address");
    ui->lineEditLabel->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditLabel->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditLabel->setGraphicsEffect(shadowEffect);

    // Amount

    ui->labelSubtitleAmount->setText("Amount");
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditAmount->setPlaceholderText("0.00");
    ui->lineEditAmount->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->layoutAmount->setGraphicsEffect(shadowEffect2);


    // Description

    ui->labelSubtitleDescription->setText("Description (optional)");
    ui->labelSubtitleDescription->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditDescription->setPlaceholderText("Add descripcion ");
    ui->lineEditDescription->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditDescription->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDescription->setGraphicsEffect(shadowEffect3);


    // Stack

    ui->stack->setCurrentIndex(pos);

    // Request QR Page

    // Address

    ui->labelAddress->setText("D7VFR83SQbiezrW72hjcWJtcfip5krte2Z ");
    ui->labelAddress->setProperty("cssClass", "text-main-grey-big");

    // QR image

    QPixmap pixmap(":/res/img/img-qr-test-big.png");
    ui->labelQrImg->setPixmap(pixmap.scaled(
                                  ui->labelQrImg->width(),
                                  ui->labelQrImg->height(),
                                  Qt::KeepAspectRatio)
                              );


    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("GENERATE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    ui->btnCopyAddress->setProperty("cssClass", "btn-primary");
    ui->btnCopyUrl->setProperty("cssClass", "btn-primary");

    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(onNextClicked()));
}


void RequestDialog::onNextClicked(){
    pos = 1;
    ui->stack->setCurrentIndex(pos);
    ui->labelTitle->setText("Request for 200 PIV");
    ui->buttonsStack->setVisible(false);
}

RequestDialog::~RequestDialog()
{
    delete ui;
}
