// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/forms/ui_sendcustomfeedialog.h"
#include "qt/pivx/qtutils.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include <QListView>
#include <QComboBox>

SendCustomFeeDialog::SendCustomFeeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCustomFeeDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");

    // Text
    ui->labelTitle->setText(tr("Customize Fee"));
    ui->labelMessage->setText(tr("Customize the transaction fee at your to your liking, depending on the fee value your transaction will be included or not in the blockchain."));
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssProperty(ui->labelMessage, "text-main-grey");

    // Recommended
    setCssProperty(ui->labelFee, "text-main-grey-big");
    setCssProperty(ui->comboBoxRecommended, "btn-combo-dialog");
    ui->comboBoxRecommended->setView(new QListView());
    ui->comboBoxRecommended->addItem(tr("Normal"), 5);
    ui->comboBoxRecommended->addItem(tr("Slow"), 20);
    ui->comboBoxRecommended->addItem(tr("Fast"), 1);

    // Custom
    setCssProperty(ui->labelCustomFee, "label-subtitle-dialog");
    ui->lineEditCustomFee->setPlaceholderText("0.000001 PIV");
    initCssEditLine(ui->lineEditCustomFee, true);

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    ui->btnSave->setText(tr("SAVE"));
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(accept()));
    connect(ui->checkBoxCustom, SIGNAL(clicked()), this, SLOT(onCustomChecked()));
    connect(ui->checkBoxRecommended, SIGNAL(clicked()), this, SLOT(onRecommendedChecked()));
    connect(ui->comboBoxRecommended, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(updateFee()));
    if(parent) connect(parent, SIGNAL(themeChanged(bool, QString&)), this, SLOT(onChangeTheme(bool, QString&)));
    ui->checkBoxRecommended->setChecked(true);
}

void SendCustomFeeDialog::setWalletModel(WalletModel* walletModel){
    this->walletModel = walletModel;
}

void SendCustomFeeDialog::showEvent(QShowEvent *event){
    updateFee();
}

void SendCustomFeeDialog::onCustomChecked(){
    bool isChecked = ui->checkBoxCustom->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(isChecked);
    ui->comboBoxRecommended->setEnabled(!isChecked);
    ui->checkBoxRecommended->setChecked(!isChecked);

    if(walletModel && ui->lineEditCustomFee->text().isEmpty()) {
        feeRate = CWallet::minTxFee;
        ui->lineEditCustomFee->setText(BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), feeRate.GetFeePerK()));
    }
}

void SendCustomFeeDialog::onRecommendedChecked(){
    bool isChecked = ui->checkBoxRecommended->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(!isChecked);
    ui->comboBoxRecommended->setEnabled(isChecked);
    ui->checkBoxCustom->setChecked(!isChecked);
}

// Fast = 1.
// Medium = 5
// Slow = 20
void SendCustomFeeDialog::updateFee(){
    if (!walletModel || !walletModel->getOptionsModel()) return;

    QVariant num = ui->comboBoxRecommended->currentData();
    bool res = false;
    int nBlocksToConfirm = num.toInt(&res);
    if (res) {
        feeRate = mempool.estimateFee(nBlocksToConfirm);
        if (feeRate <= CFeeRate(0)) { // not enough data => minfee
            feeRate = CWallet::minTxFee;
            ui->labelFee->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                               feeRate.GetFeePerK()) + "/kB");
        } else {
            ui->labelFee->setText(
                    BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                 feeRate.GetFeePerK()) + "/kB");
        }
    }
}

void SendCustomFeeDialog::clear(){
    onRecommendedChecked();
    updateFee();
}

CFeeRate SendCustomFeeDialog::getFeeRate(){
    return ui->checkBoxRecommended->isChecked() ?
           feeRate : CFeeRate(GUIUtil::parseValue(ui->lineEditCustomFee->text(), walletModel->getOptionsModel()->getDisplayUnit()));
}

void SendCustomFeeDialog::onChangeTheme(bool isLightTheme, QString& theme){
    this->setStyleSheet(theme);
    updateStyle(this);
}

SendCustomFeeDialog::~SendCustomFeeDialog(){
    delete ui;
}
