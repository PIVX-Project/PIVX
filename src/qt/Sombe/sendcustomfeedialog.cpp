//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/sendcustomfeedialog.h"
#include "qt/Sombe/forms/ui_sendcustomfeedialog.h"
#include "qt/Sombe/qtutils.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include <QListView>
#include <QComboBox>

SendCustomFeeDialog::SendCustomFeeDialog(SombeGUI* parent, WalletModel* model) :
    FocusedDialog(parent),
    ui(new Ui::SendCustomFeeDialog),
    walletModel(model)
{
    if (!walletModel)
        throw std::runtime_error(strprintf("%s: No wallet model set", __func__));
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");

    // Text
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
    initCssEditLine(ui->lineEditCustomFee, true);
    GUIUtil::setupAmountWidget(ui->lineEditCustomFee, this);

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, &QPushButton::clicked, this, &SendCustomFeeDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SendCustomFeeDialog::close);
    connect(ui->btnSave, &QPushButton::clicked, this, &SendCustomFeeDialog::accept);
    connect(ui->checkBoxCustom, &QCheckBox::clicked, this, &SendCustomFeeDialog::onCustomChecked);
    connect(ui->checkBoxRecommended, &QCheckBox::clicked, this, &SendCustomFeeDialog::onRecommendedChecked);
    connect(ui->comboBoxRecommended, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
        this, &SendCustomFeeDialog::updateFee);
    if (parent)
        connect(parent, &SombeGUI::themeChanged, this, &SendCustomFeeDialog::onChangeTheme);
    ui->checkBoxRecommended->setChecked(true);
}

void SendCustomFeeDialog::showEvent(QShowEvent* event)
{
    FocusedDialog::showEvent(event);
    updateFee();
    if (walletModel->hasWalletCustomFee()) {
        ui->checkBoxCustom->setChecked(true);
        onCustomChecked();
    } else {
        ui->checkBoxRecommended->setChecked(true);
        onRecommendedChecked();
    }
}

void SendCustomFeeDialog::onCustomChecked()
{
    bool isChecked = ui->checkBoxCustom->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(isChecked);
    ui->comboBoxRecommended->setEnabled(!isChecked);
    ui->checkBoxRecommended->setChecked(!isChecked);

    if (isChecked) {
        CAmount nFee;
        walletModel->getWalletCustomFee(nFee);
        ui->lineEditCustomFee->setText(BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), nFee));
    } else {
        ui->lineEditCustomFee->clear();
    }
}

void SendCustomFeeDialog::onRecommendedChecked()
{
    bool isChecked = ui->checkBoxRecommended->checkState() == Qt::Checked;
    ui->lineEditCustomFee->setEnabled(!isChecked);
    ui->comboBoxRecommended->setEnabled(isChecked);
    ui->checkBoxCustom->setChecked(!isChecked);
    if (isChecked) {
        ui->lineEditCustomFee->clear();
    }
}

// Fast = 1.
// Medium = 5
// Slow = 20
void SendCustomFeeDialog::updateFee()
{
    if (!walletModel->getOptionsModel()) return;

    QVariant num = ui->comboBoxRecommended->currentData();
    bool res = false;
    int nBlocksToConfirm = num.toInt(&res);
    if (res) {
        feeRate = mempool.estimateFee(nBlocksToConfirm);
        if (feeRate < CWallet::minTxFee) feeRate = CWallet::minTxFee;    // not enough data => minfee
        ui->labelFee->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                           feeRate.GetFeePerK()) + "/kB");
    }
}

void SendCustomFeeDialog::accept()
{
    const bool fUseCustomFee = ui->checkBoxCustom->checkState() == Qt::Checked;
    const CAmount customFee = getFeeRate().GetFeePerK();
    // Check insane fee
    const CAmount insaneFee = ::minRelayTxFee.GetFeePerK() * 10000;
    if (customFee >= insaneFee) {
        inform(tr("Fee too high. Must be below: %1").arg(
                BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), insaneFee)));
    } else if (customFee < CWallet::GetRequiredFee(1000)) {
        inform(tr("Fee too low. Must be at least: %1").arg(
                BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), CWallet::GetRequiredFee(1000))));
    } else {
        walletModel->setWalletCustomFee(fUseCustomFee, customFee);
        QDialog::accept();
    }
}

void SendCustomFeeDialog::clear()
{
    ui->comboBoxRecommended->setCurrentIndex(0);
}

CFeeRate SendCustomFeeDialog::getFeeRate()
{
    return ui->checkBoxRecommended->isChecked() ?
           feeRate : CFeeRate(GUIUtil::parseValue(ui->lineEditCustomFee->text(), walletModel->getOptionsModel()->getDisplayUnit()));
}

bool SendCustomFeeDialog::isCustomFeeChecked()
{
    return ui->checkBoxCustom->checkState() == Qt::Checked;
}

void SendCustomFeeDialog::onChangeTheme(bool isLightTheme, QString& theme)
{
    this->setStyleSheet(theme);
    updateStyle(this);
}

void SendCustomFeeDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

SendCustomFeeDialog::~SendCustomFeeDialog()
{
    delete ui;
}
