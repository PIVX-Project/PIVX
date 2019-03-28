#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/forms/ui_sendconfirmdialog.h"

SendConfirmDialog::SendConfirmDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendConfirmDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Title

    ui->labelTitle->setText("Confirm your transaction");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Labels

    ui->labelAmount->setProperty("cssClass", "text-body1-dialog");
    ui->labelSend->setProperty("cssClass", "text-body1-dialog");
    ui->labelInputs->setProperty("cssClass", "text-body1-dialog");
    ui->labelFee->setProperty("cssClass", "text-body1-dialog");
    ui->labelChange->setProperty("cssClass", "text-body1-dialog");

    ui->contentInputs->setProperty("cssClass", "layout-arrow");

    ui->labelDivider2->setProperty("cssClass", "container-divider");
    ui->labelDivider3->setProperty("cssClass", "container-divider");
    ui->labelDivider4->setProperty("cssClass", "container-divider");
    ui->labelDivider6->setProperty("cssClass", "container-divider");


    // Content

    ui->textAmount->setProperty("cssClass", "text-body1-dialog");
    ui->textSend->setProperty("cssClass", "text-body1-dialog");
    ui->textInputs->setProperty("cssClass", "text-body1-dialog");
    ui->textFee->setProperty("cssClass", "text-body1-dialog");
    ui->textChange->setProperty("cssClass", "text-body1-dialog");


    // Buttons
    
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));

    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

SendConfirmDialog::~SendConfirmDialog()
{
    delete ui;
}
