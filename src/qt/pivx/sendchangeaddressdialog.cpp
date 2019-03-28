#include "qt/pivx/sendchangeaddressdialog.h"
#include "qt/pivx/forms/ui_sendchangeaddressdialog.h"
#include "QGraphicsDropShadowEffect"

SendChangeAddressDialog::SendChangeAddressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendChangeAddressDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Text

    ui->labelTitle->setText("Custom Change Address");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");


    // Custom

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditAddress->setPlaceholderText("Ente a PIVX  address (e.g D7VFR83SQbiezrW72hjc… ");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAddress->setGraphicsEffect(shadowEffect);


    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}



SendChangeAddressDialog::~SendChangeAddressDialog()
{
    delete ui;
}
