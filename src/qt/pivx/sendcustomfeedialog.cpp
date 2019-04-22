#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/forms/ui_sendcustomfeedialog.h"
#include "QListView"
#include "QGraphicsDropShadowEffect"
#include "qt/pivx/qtutils.h"

SendCustomFeeDialog::SendCustomFeeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCustomFeeDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Text

    ui->labelTitle->setText("Customize Fee");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("This will unlock your wallet fully, so that anyone with access to it can spend until the wallet is closed or locked again.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Recommended

    ui->labelFee->setText("0.0000026896 zPIV/KB");
    ui->labelFee->setProperty("cssClass", "text-main-grey-big");

    ui->comboBoxRecommended->setProperty("cssClass", "btn-combo-dialog");
    ui->comboBoxRecommended->setView(new QListView());

    ui->comboBoxRecommended->addItem("Normal");
    ui->comboBoxRecommended->addItem("Slow");
    ui->comboBoxRecommended->addItem("Fast");

    // Custom

    ui->comboBoxCustom->setProperty("cssClass", "btn-combo-dialog");
    ui->comboBoxCustom->setView(new QListView());
    ui->comboBoxCustom->addItem("Per kilobyte");

    ui->lineEditCustomFee->setPlaceholderText("0.000001 zPIV");
    ui->lineEditCustomFee->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditCustomFee->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditCustomFee);

    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->checkBoxCustom, SIGNAL(clicked()), this, SLOT(onCustomChecked()));
    connect(ui->checkBoxRecommended, SIGNAL(clicked()), this, SLOT(onRecommendedChecked()));
    ui->checkBoxRecommended->setChecked(true);
}

void SendCustomFeeDialog::onCustomChecked(){
    bool isChecked = ui->checkBoxCustom->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(isChecked);
    ui->comboBoxCustom->setEnabled(isChecked);
    ui->comboBoxRecommended->setEnabled(!isChecked);
    ui->checkBoxRecommended->setChecked(!isChecked);

}

void SendCustomFeeDialog::onRecommendedChecked(){
    bool isChecked = ui->checkBoxRecommended->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(!isChecked);
    ui->comboBoxCustom->setEnabled(!isChecked);
    ui->comboBoxRecommended->setEnabled(isChecked);
    ui->checkBoxCustom->setChecked(!isChecked);
}

SendCustomFeeDialog::~SendCustomFeeDialog()
{
    delete ui;
}
