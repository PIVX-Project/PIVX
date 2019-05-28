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
    ui->labelTitle->setText(tr("New Request Payment"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    ui->labelMessage->setText(tr("Instead of sending somebody a PIVX address and asking them to pay to that address, you give them a Payment Request message which bundles up more information than is contained in just a PIVX address."));
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Container
    ui->frame->setProperty("cssClass", "container-dialog");

    // Combo Coins
    ui->comboBoxCoin->setProperty("cssClass", "btn-combo-coins");
    ui->comboContainer->setProperty("cssClass", "container-purple");

    QListView * listView = new QListView();
    ui->comboBoxCoin->addItem("PIV");
    ui->comboBoxCoin->addItem("zPIV");
    ui->comboBoxCoin->setView(listView);

    // Label
    ui->labelSubtitleLabel->setText(tr("Label"));
    ui->labelSubtitleLabel->setProperty("cssClass", "text-title2-dialog");
    ui->lineEditLabel->setPlaceholderText(tr("Enter a label to be saved withing the address"));
    setCssEditLineDialog(ui->lineEditLabel, true);
    ui->lineEditLabel->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditLabel);

    // Amount
    ui->labelSubtitleAmount->setText(tr("Amount"));
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title2-dialog");
    ui->lineEditAmount->setPlaceholderText("0.00 PIV");
    setCssEditLineDialog(ui->lineEditAmount, true);
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->layoutAmount);

    // Description
    ui->labelSubtitleDescription->setText(tr("Description (optional)"));
    ui->labelSubtitleDescription->setProperty("cssClass", "text-title2-dialog");

    ui->lineEditDescription->setPlaceholderText(tr("Add description "));
    setCssEditLineDialog(ui->lineEditDescription, true);
    ui->lineEditDescription->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditDescription);

    // Stack
    ui->stack->setCurrentIndex(pos);
    // Request QR Page
    // Address
    ui->labelAddress->setText(tr("Error"));
    ui->labelAddress->setProperty("cssClass", "text-main-grey-big");

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText(tr("GENERATE"));
    setCssBtnPrimary(ui->btnSave);
    setCssBtnPrimary(ui->btnCopyAddress);
    setCssBtnPrimary(ui->btnCopyUrl);

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
        ui->labelTitle->setText("Request for " + BitcoinUnits::format(displayUnit, value, false, BitcoinUnits::separatorAlways) + " PIV");
        updateQr(info->address);
        ui->labelAddress->setText(info->address);
        ui->buttonsStack->setVisible(false);
        pos = 1;
        ui->stack->setCurrentIndex(pos);
    }
}

void RequestDialog::onCopyClicked(){
    if(info) {
        GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(*info));
        static_cast<PIVXGUI*>(parentWidget())->messageInfo(tr("URI copied to clipboard"));
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
