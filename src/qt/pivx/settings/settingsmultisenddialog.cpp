#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/settings/forms/ui_settingsmultisenddialog.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

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

    ui->labelTitle->setText("New recipient for multisend");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // CheckBox

    ui->checkBoxStake->setText("Stakes");
    ui->checkBoxRewards->setText("Masternode rewards");


    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22)
);
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


    // Label

    ui->labelSubtitleLabel->setText("Label (optional)");
    ui->labelSubtitleLabel->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditLabel->setPlaceholderText("Enter a label to add this address in your address book");
    ui->lineEditLabel->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditLabel->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditLabel->setGraphicsEffect(shadowEffect);


    // Address

    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditAddress->setPlaceholderText("e.g D7VFR83SQbiezrW72hjc… ");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAddress->setGraphicsEffect(shadowEffect2);


    // Combobox

    ui->labelSubtitlePercentage->setText("Porcentage");
    ui->labelSubtitlePercentage->setProperty("cssClass", "text-title2-dialog");


    ui->comboBoxPercentage->setProperty("cssClass", "btn-combo-edit-dialog");
    ui->comboBoxPercentage->setGraphicsEffect(shadowEffect3);

    QListView * listViewStake = new QListView();
    ui->comboBoxPercentage->setView(listViewStake);

    ui->comboBoxPercentage->addItem("8%");
    ui->comboBoxPercentage->addItem("10%");



    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("ACTIVATE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

SettingsMultisendDialog::~SettingsMultisendDialog()
{
    delete ui;
}
