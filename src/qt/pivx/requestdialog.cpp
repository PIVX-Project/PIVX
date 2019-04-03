#include "qt/pivx/requestdialog.h"
#include "qt/pivx/forms/ui_requestdialog.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "amount.h"
#include "optionsmodel.h"

RequestDialog::RequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RequestDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Text

    ui->labelTitle->setText("New Request Payment");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    // Combo Coins

    ui->comboBoxCoin->setProperty("cssClass", "btn-combo-coins");
    ui->comboContainer->setProperty("cssClass", "container-purple");

    QListView * listView = new QListView();

    ui->comboBoxCoin->addItem("PIV");
    ui->comboBoxCoin->addItem("zPIV");
    ui->comboBoxCoin->setView(listView);


    // Label

    ui->labelSubtitleLabel->setText("Label");
    ui->labelSubtitleLabel->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditLabel->setPlaceholderText("Enter a label to be saved withing the address");
    setCssEditLineDialog(ui->lineEditLabel, true);

    ui->lineEditLabel->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditLabel->setGraphicsEffect(shadowEffect);

    // Amount

    ui->labelSubtitleAmount->setText("Amount");
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditAmount->setPlaceholderText("0.00");
    setCssEditLineDialog(ui->lineEditAmount, true);
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->layoutAmount->setGraphicsEffect(shadowEffect);


    // Description

    ui->labelSubtitleDescription->setText("Description (optional)");
    ui->labelSubtitleDescription->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditDescription->setPlaceholderText("Add descripcion ");
    setCssEditLineDialog(ui->lineEditDescription, true);
    ui->lineEditDescription->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDescription->setGraphicsEffect(shadowEffect);


    // Stack

    ui->stack->setCurrentIndex(pos);

    // Request QR Page

    // Address

    ui->labelAddress->setText("D7VFR83SQbiezrW72hjcWJtcfip5krte2Z ");
    ui->labelAddress->setProperty("cssClass", "text-main-grey-big");

    // QR image

    QPixmap pixmap(":/res/img/img-qr-test-big.png");
    ui->labelQrImg->setPixmap(pixmap.scaled(
                                  ui->labelQrImg->width(),
                                  ui->labelQrImg->height(),
                                  Qt::KeepAspectRatio)
                              );


    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("GENERATE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    ui->btnCopyAddress->setProperty("cssClass", "btn-primary");
    ui->btnCopyUrl->setProperty("cssClass", "btn-primary");

    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(onNextClicked()));

    // TODO: Change copy address for save image (the method is already implemented in other class called exportQr or something like that)
    connect(ui->btnCopyAddress, SIGNAL(clicked()), this, SLOT(onCopyClicked()));
}

void RequestDialog::setWalletModel(WalletModel *model){
    this->walletModel = model;
}


void RequestDialog::onNextClicked(){

    if(walletModel) {
        // info
        info = new SendCoinsRecipient();


        info->label = ui->lineEditLabel->text();
        info->message = ui->lineEditDescription->text();
        info->address = QString::fromStdString(walletModel->getNewAddress().ToString());
        int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        bool isValueValid = true;
        CAmount value = GUIUtil::parseValue(
                ui->lineEditAmount->text(),
                displayUnit,
                &isValueValid
        );
        info->amount = value;

        if(value <= 0 || !isValueValid){
            // TODO: Notify problem..
            return;
        }
        // TODO: validate address etc etc.

        // TODO: Complete amount and QR.

        ui->labelTitle->setText("Request for " + BitcoinUnits::format(displayUnit, value, false, BitcoinUnits::separatorAlways));
        updateQr(info->address);
        ui->buttonsStack->setVisible(false);
        pos = 1;
        ui->stack->setCurrentIndex(pos);
    }
}

void RequestDialog::onCopyClicked(){
    if(info) {
        GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(*info));
        // TODO: Notify
        //window->messageInfo(tr("Address copied"));
        close();
    }
}

void RequestDialog::updateQr(QString str){
    QString uri = GUIUtil::formatBitcoinURI(*info);
    ui->labelQrImg->setText("");
    QString error;
    QPixmap pixmap = encodeToQr(uri, error);
    if(!pixmap.isNull()){
        qrImage = &pixmap;
        ui->labelQrImg->setPixmap(qrImage->scaled(ui->labelQrImg->width(), ui->labelQrImg->height()));
    }else{
        ui->labelQrImg->setText(!error.isEmpty() ? error : "Error encoding address");
    }
}

RequestDialog::~RequestDialog()
{
    delete ui;
}
