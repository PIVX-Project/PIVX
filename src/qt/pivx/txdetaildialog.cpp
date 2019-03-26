#include "qt/pivx/txdetaildialog.h"
#include "qt/pivx/forms/ui_txdetaildialog.h"

TxDetailDialog::TxDetailDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TxDetailDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Title
    ui->labelTitle->setText("Transaction Details");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Labels

    ui->labelId->setProperty("cssClass", "text-body1-dialog");
    ui->labelAmount->setProperty("cssClass", "text-body1-dialog");
    ui->labelSend->setProperty("cssClass", "text-body1-dialog");
    ui->labelInputs->setProperty("cssClass", "text-body1-dialog");
    ui->labelFee->setProperty("cssClass", "text-body1-dialog");
    ui->labelConfirmations->setProperty("cssClass", "text-body1-dialog");
    ui->labelSize->setProperty("cssClass", "text-body1-dialog");
    ui->labelDate->setProperty("cssClass", "text-body1-dialog");
    ui->labelStatus->setProperty("cssClass", "text-body1-dialog");


    ui->contentSent->setProperty("cssClass", "layout-arrow");
    ui->contentInputs->setProperty("cssClass", "layout-arrow");

    ui->labelDivider1->setProperty("cssClass", "container-divider");
    ui->labelDivider2->setProperty("cssClass", "container-divider");
    ui->labelDivider3->setProperty("cssClass", "container-divider");
    ui->labelDivider4->setProperty("cssClass", "container-divider");
    ui->labelDivider5->setProperty("cssClass", "container-divider");
    ui->labelDivider6->setProperty("cssClass", "container-divider");
    ui->labelDivider7->setProperty("cssClass", "container-divider");


    // Content

    ui->textId->setProperty("cssClass", "text-body1-dialog");
    ui->textAmount->setProperty("cssClass", "text-body1-dialog");
    ui->textSend->setProperty("cssClass", "text-body1-dialog");
    ui->textInputs->setProperty("cssClass", "text-body1-dialog");
    ui->textFee->setProperty("cssClass", "text-body1-dialog");
    ui->textConfirmations->setProperty("cssClass", "text-body1-dialog");
    ui->textSize->setProperty("cssClass", "text-body1-dialog");
    ui->textDate->setProperty("cssClass", "text-body1-dialog");
    ui->textStatus->setProperty("cssClass", "text-body1-dialog");

    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
}

TxDetailDialog::~TxDetailDialog()
{
    delete ui;
}
