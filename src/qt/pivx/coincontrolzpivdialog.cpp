#include "qt/pivx/coincontrolzpivdialog.h"
#include "qt/pivx/forms/ui_coincontrolzpivdialog.h"

CoinControlZpivDialog::CoinControlZpivDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CoinControlZpivDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frameContainer->setProperty("cssClass", "container-dialog");
    ui->layoutAmount->setProperty("cssClass", "container-border-purple");
    ui->layoutQuantity->setProperty("cssClass", "container-border-purple");

    // Title

    ui->labelTitle->setText("Select PIV Denominations to Spend");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Label Style

    ui->labelTitleAmount->setProperty("cssClass", "text-main-purple");
    ui->labelTitleConfirmations->setProperty("cssClass", "text-main-purple");
    ui->labelTitleDenom->setProperty("cssClass", "text-main-purple");
    ui->labelTitleId->setProperty("cssClass", "text-main-purple");
    ui->labelTitleQuantity->setProperty("cssClass", "text-main-purple");
    ui->labelTitleSpen->setProperty("cssClass", "text-main-purple");
    ui->labelTitleVersion->setProperty("cssClass", "text-main-purple");

    ui->labelValueAmount->setProperty("cssClass", "text-main-purple");
    ui->labelValueQuantity->setProperty("cssClass", "text-main-purple");


    // Values

    ui->labelValueAmount->setText("0");
    ui->labelValueQuantity->setText("0");

    // Buttons


    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

CoinControlZpivDialog::~CoinControlZpivDialog()
{
    delete ui;
}
