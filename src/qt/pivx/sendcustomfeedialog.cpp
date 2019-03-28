#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/forms/ui_sendcustomfeedialog.h"
#include "QListView"
#include "QGraphicsDropShadowEffect"

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

    QListView * listViewRecommended = new QListView();
    ui->comboBoxRecommended->setView(listViewRecommended);

    ui->comboBoxRecommended->addItem("Normal");
    ui->comboBoxRecommended->addItem("Slow");
    ui->comboBoxRecommended->addItem("Fast");


    // Custom

    ui->comboBoxCustom->setProperty("cssClass", "btn-combo-dialog");

    QListView * listViewCustom = new QListView();
    ui->comboBoxCustom->setView(listViewCustom);

    ui->comboBoxCustom->addItem("Per kilobyte");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditCustomFee->setPlaceholderText("0.000001 zPIV");
    ui->lineEditCustomFee->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditCustomFee->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditCustomFee->setGraphicsEffect(shadowEffect);




    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

SendCustomFeeDialog::~SendCustomFeeDialog()
{
    delete ui;
}
