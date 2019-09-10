// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/settings/forms/ui_settingsmultisenddialog.h"
#include <QListView>
#include <QIntValidator>
#include "qt/pivx/qtutils.h"

SettingsMultisendDialog::SettingsMultisendDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsMultisendDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container
    setCssProperty(ui->frame, "container-dialog");

    // Text
    ui->labelTitle->setText(tr("New recipient for multisend"));
    setCssProperty(ui->labelTitle, "text-title-dialog");

    // Label
    ui->labelSubtitleLabel->setText(tr("Label (optional)"));
    setCssProperty(ui->labelSubtitleLabel, "text-title2-dialog");

    ui->lineEditLabel->setPlaceholderText(tr("Enter a label to add this address in your address book"));
    initCssEditLine(ui->lineEditLabel, true);

    // Address
    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    setCssProperty(ui->labelSubtitleAddress, "text-title2-dialog");
    ui->lineEditAddress->setPlaceholderText("e.g D7VFR83SQbiezrW72hjc… ");
    initCssEditLine(ui->lineEditAddress, true);
    ui->lineEditAddress->setValidator(new QRegExpValidator(QRegExp("^[A-Za-z0-9]+"), ui->lineEditAddress));


    ui->labelSubtitlePercentage->setText(tr("Percentage"));
    setCssProperty(ui->labelSubtitlePercentage, "text-title2-dialog");
    ui->lineEditPercentage->setPlaceholderText("10%");
    initCssEditLine(ui->lineEditPercentage, true);
    ui->lineEditPercentage->setValidator(new QIntValidator(0, 100, ui->lineEditPercentage));

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    ui->btnSave->setText("ADD");
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, &QPushButton::clicked, [this](){
        this->isOk = true;
        accept();
    });
}

QString SettingsMultisendDialog::getAddress(){
    return ui->lineEditAddress->text();
}
QString SettingsMultisendDialog::getLabel(){
    return ui->lineEditLabel->text();
}
int SettingsMultisendDialog::getPercentage(){
    QString percentage = ui->lineEditPercentage->text();
    if (percentage.isEmpty()) return 0;
    return percentage.toInt();
}

SettingsMultisendDialog::~SettingsMultisendDialog(){
    delete ui;
}
