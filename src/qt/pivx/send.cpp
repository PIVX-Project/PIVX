#include "qt/pivx/send.h"
#include "qt/pivx/forms/ui_send.h"

#include "qt/pivx/qtutils.h"
#include "qt/pivx/sendchangeaddressdialog.h"
#include "qt/pivx/optionbutton.h"
#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/coincontrolzpivdialog.h"
#include "qt/pivx/coincontrolpivwidget.h"
#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/myaddressrow.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>

#include <iostream>

SendWidget::SendWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::send),
    window(_window),
    coinIcon(new QPushButton()),
    btnContacts(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,20,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    /* Title */
    ui->labelTitle->setText("Send");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText("PIV");
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText("zPIV");
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    /* Subtitle */

    ui->labelSubtitle1->setText("You can transfer public coins: PIV or private ones: zPIV");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle2->setText("Select coin type to spend");
    ui->labelSubtitle2->setProperty("cssClass", "text-subtitle");

    /* Address */

    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title");


    /* Amount */

    ui->labelSubtitleAmount->setText("Amount");
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title");

    // Buttons

    ui->pushButtonFee->setText("Standard Fee 0.000005 PIV");
    ui->pushButtonFee->setProperty("cssClass", "btn-secundary");

    ui->pushButtonClear->setText("Clear all");
    ui->pushButtonClear->setProperty("cssClass", "btn-secundary-clear");

    ui->pushButtonAddRecipient->setText("Add recipient");
    ui->pushButtonAddRecipient->setProperty("cssClass", "btn-secundary-add");

    ui->pushButtonSave->setText("Send zPIV");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

    // Coin control
    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Control coin");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select the source of the coins.");

    // Change eddress option
    ui->btnChangeAddress->setTitleClassAndText("btn-title-grey", "Change address");
    ui->btnChangeAddress->setSubTitleClassAndText("text-subtitle", "Customize the change address.");

    connect(ui->pushButtonFee, SIGNAL(clicked()), this, SLOT(onChangeCustomFeeClicked()));
    connect(ui->btnCoinControl, SIGNAL(clicked()), this, SLOT(onCoinControlClicked()));
    connect(ui->btnChangeAddress, SIGNAL(clicked()), this, SLOT(onChangeAddressClicked()));

    ui->coinWidget->setProperty("cssClass", "container-coin-type");
    ui->labelLine->setProperty("cssClass", "container-divider");


    // Total Send

    ui->labelTitleTotalSend->setText("Total to send");
    ui->labelTitleTotalSend->setProperty("cssClass", "text-title");

    ui->labelAmountSend->setText("0.0 zPIV");
    ui->labelAmountSend->setProperty("cssClass", "text-body1");

    // Total Remaining

    ui->labelTitleTotalRemaining->setText("Total remaining");
    ui->labelTitleTotalRemaining->setProperty("cssClass", "text-title");

    ui->labelAmountRemaining->setText("1000 zPIV");
    ui->labelAmountRemaining->setProperty("cssClass", "text-body1");

    // Contact Button
/*
    btnContacts->setProperty("cssClass", "btn-dropdown");
    btnContacts->setCheckable(true);

    QSize BUTTON_CONTACT_SIZE = QSize(24, 24);
    btnContacts->setMinimumSize(BUTTON_CONTACT_SIZE);
    btnContacts->setMaximumSize(BUTTON_CONTACT_SIZE);

    ui->stackedWidget_2->addWidget(btnContacts);

    btnContacts->show();
    btnContacts->raise();

    int posXX = ui->lineEditAddress->width() - 30;
    int posYY = 12;
    btnContacts->move(450, posYY);
*/

    // Icon Send
    ui->stackedWidget->addWidget(coinIcon);
    coinIcon->show();
    coinIcon->raise();

    coinIcon->setProperty("cssClass", "coin-icon-zpiv");

    QSize BUTTON_SIZE = QSize(24, 24);
    coinIcon->setMinimumSize(BUTTON_SIZE);
    coinIcon->setMaximumSize(BUTTON_SIZE);

    int posX = 0;
    int posY = 20;
    coinIcon->move(posX, posY);

    // Entry
    addEntry();

    // Connect
    connect(ui->pushLeft, SIGNAL(clicked()), this, SLOT(onPIVSelected()));
    connect(ui->pushRight, SIGNAL(clicked()), this, SLOT(onzPIVSelected()));
    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(btnContacts, SIGNAL(clicked()), this, SLOT(onContactsClicked()));

    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
    connect(ui->pushButtonAddRecipient, SIGNAL(clicked()), this, SLOT(onAddEntryClicked()));
    connect(ui->pushButtonClear, SIGNAL(clicked()), this, SLOT(clearEntries()));
}

void SendWidget::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    if (clientModel) {
        // TODO: Complete me..
        //connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(updateSmartFeeLabel()));
    }
}

void SendWidget::setModel(WalletModel* model) {
    this->walletModel = model;

    if (model && model->getOptionsModel()) {
        for(SendMultiRow *entry : entries){
            if(entry){
                // TODO: complete me?
                //entry->setModel();
            }
        }

        // TODO: Unit display complete me
        //connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        //updateDisplayUnit();

        // TODO: Coin control complet eme
        // Coin Control
        //connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        //connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        //ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        //coinControlUpdateLabels();

        // TODO: fee section, check sendDialog, same method

    }


}

void SendWidget::clearEntries(){
    int num = entries.size();
    for (int i = 0; i < num; ++i) {
        ui->scrollAreaWidgetContents->layout()->takeAt(0)->widget()->deleteLater();
    }
    entries.clear();

    addEntry();
}

void SendWidget::addEntry(){
    if(entries.empty()){
        SendMultiRow *sendMultiRow = new SendMultiRow(this);
        entries.push_back(sendMultiRow);
        ui->scrollAreaWidgetContents->layout()->addWidget(sendMultiRow);
    } else {
        if (entries.size() == 1) {
            SendMultiRow *entry = entries.front();
            entry->hideLabels();
            entry->setNumber(1);
        }else if(entries.size() == MAX_SEND_POPUP_ENTRIES){
            // TODO: Snackbar notifying that it surpassed the max amount of entries
            return;
        }

        SendMultiRow *sendMultiRow = new SendMultiRow(this);
        entries.push_back(sendMultiRow);
        sendMultiRow->setNumber(entries.size());
        sendMultiRow->hideLabels();
        ui->scrollAreaWidgetContents->layout()->addWidget(sendMultiRow);
    }
}

void SendWidget::onAddEntryClicked(){
    // TODO: Validations here..
    addEntry();
}

void SendWidget::resizeEvent(QResizeEvent *event)
 {
    /*
    int posXX = ui->lineEditAddress->width() - 30;
    int posYY = 12;
    btnContacts->move(posXX, posYY);
    */
    //resizeMenu();
    QWidget::resizeEvent(event);
 }


void SendWidget::onSendClicked(){

    if (!walletModel || !walletModel->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;

    for (SendMultiRow* entry : entries){
        // TODO: Check what is the UTXO splitter here..

        // Validate send..
        if(entry && entry->validate()) {
            recipients.append(entry->getValue());
        }else{
            // Invalid entry.. todo: notificate user about this.
            return;
        }

    }

    if (recipients.isEmpty()) {
        //todo: notificate user about this.
        return;
    }

    // request unlock only if was locked or unlocked for mixing:
    // this way we let users unlock by walletpassphrase or by menu
    // and make many transactions while unlocking through this dialog
    // will call relock
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Send_PIV, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            //TODO: Check what is this --> fNewRecipientAllowed = true;
            // TODO: Notify the user..
            return;
        }
        //send(recipients, strFee, formatted);
        return;
    }

    // already unlocked or not encrypted at all
    //send(recipients, strFee, formatted);


    /*
    window->showHide(true);
    SendConfirmDialog* dialog = new SendConfirmDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
     */

}


void SendWidget::onChangeAddressClicked(){
    window->showHide(true);
    SendChangeAddressDialog* dialog = new SendChangeAddressDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

}

void SendWidget::onChangeCustomFeeClicked(){
    window->showHide(true);
    SendCustomFeeDialog* dialog = new SendCustomFeeDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
}

void SendWidget::onCoinControlClicked()
{
    window->showHide(true);
    if(isPIV){
        CoinControlPivWidget* dialog = new CoinControlPivWidget(window);
        openDialogWithOpaqueBackgroundY(dialog, window, 6.5, 5);
    }else{
        CoinControlZpivDialog* dialog = new CoinControlZpivDialog(window);
        openDialogWithOpaqueBackgroundY(dialog, window, 6.5, 5);
    }
}

void SendWidget::onPIVSelected(){
    isPIV = true;
    coinIcon->setProperty("cssClass", "coin-icon-piv");
    updateStyle(coinIcon);
}

void SendWidget::onzPIVSelected(){
    isPIV = false;
    coinIcon->setProperty("cssClass", "coin-icon-zpiv");
    updateStyle(coinIcon);
}


void SendWidget::onContactsClicked(){

    /*
    int height = ui->stackedWidget_2->height() * 8;
    int width = ui->stackedWidget_2->width();

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this
                    );

    }else{
        menuContacts->setMinimumHeight(height);
        menuContacts->setMinimumWidth(width);
        menuContacts->resizeList(width, height);
        menuContacts->resize(width, height);
    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }

    menuContacts->setStyleSheet(this->styleSheet());

    QRect rect = ui->lineEditAddress->rect();
    QPoint pos = rect.bottomLeft();
    // TODO: Change this position..
    pos.setX(pos.x() + 20);
    pos.setY(pos.y() + ((ui->lineEditAddress->height() - 4)  * 3));
    menuContacts->move(pos);
    menuContacts->show();
    */
}

void SendWidget::resizeMenu(){
    /*
    if(menuContacts && menuContacts->isVisible()){
        int width = ui->stackedWidget_2->width();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QRect rect = ui->lineEditAddress->rect();
        QPoint pos = rect.bottomLeft();
        // TODO: Change this position..
        pos.setX(pos.x() + 20);
        pos.setY(pos.y() + ((ui->lineEditAddress->height() - 4)  * 3));
        menuContacts->move(pos);
    }
    */
}

void SendWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    if(this->menuContacts){
        this->menuContacts->setStyleSheet(theme);
        if(this->menuContacts->isVisible()){
            updateStyle(menuContacts);
        }
    }
    updateStyle(this);
}

SendWidget::~SendWidget()
{
    delete ui;
}
