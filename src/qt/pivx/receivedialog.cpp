#include "qt/pivx/receivedialog.h"
#include "qt/pivx/forms/ui_receivedialog.h"
#include <QFile>

ReceiveDialog::ReceiveDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveDialog)
{
    ui->setupUi(this);
    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    ui->frameContainer->setProperty("cssClass", "container-dialog");
    ui->frameContainer->setContentsMargins(10,10,10,10);


    // Title

    ui->labelTitle->setText("My Address");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

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
    ui->btnSave->setText("COPY");
    ui->btnSave->setProperty("cssClass", "btn-primary");
    ui->btnCancel->setVisible(false);


    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(onCopy()));
}

void ReceiveDialog::onCopy(){
    // TODO: copy to clipboard..
    accept();
}

ReceiveDialog::~ReceiveDialog()
{
    delete ui;
}
