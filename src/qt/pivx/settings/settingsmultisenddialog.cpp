#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/settings/forms/ui_settingsmultisenddialog.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"
#include "qt/pivx/qtutils.h"

SettingsMultisendDialog::SettingsMultisendDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsMultisendDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container
    ui->frame->setProperty("cssClass", "container-dialog");

    // Text
    ui->labelTitle->setText(tr("New recipient for multisend"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // CheckBox
    ui->checkBoxStake->setText(tr("Stakes"));
    ui->checkBoxRewards->setText(tr("Masternode rewards"));

    // Label
    ui->labelSubtitleLabel->setText(tr("Label (optional)"));
    ui->labelSubtitleLabel->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditLabel->setPlaceholderText(tr("Enter a label to add this address in your address book"));
    initCssEditLine(ui->lineEditLabel, true);


    // Address
    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditAddress->setPlaceholderText("e.g D7VFR83SQbiezrW72hjc… ");
    initCssEditLine(ui->lineEditAddress, true);

    // Combobox
    ui->labelSubtitlePercentage->setText(tr("Percentage"));
    ui->labelSubtitlePercentage->setProperty("cssClass", "text-title2-dialog");


    ui->comboBoxPercentage->setProperty("cssClass", "btn-combo-edit-dialog");
    setShadow(ui->comboBoxPercentage);
    ui->comboBoxPercentage->setView(new QListView());

    ui->comboBoxPercentage->addItem("8%");
    ui->comboBoxPercentage->addItem("10%");

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");
    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("ADD");
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

SettingsMultisendDialog::~SettingsMultisendDialog()
{
    delete ui;
}
