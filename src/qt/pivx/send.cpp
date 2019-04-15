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
#include "optionsmodel.h"
#include "coincontrol.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>

#include <iostream>

SendWidget::SendWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::send),
    window(_window),
    coinIcon(new QPushButton()),
    btnContacts(new QPushButton()),
    coinControl(new CCoinControl())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,20);
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
    ui->pushLeft->setChecked(true);
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

    ui->labelAmountSend->setText("0.00 zPIV");
    ui->labelAmountSend->setProperty("cssClass", "text-body1");

    // Total Remaining

    ui->labelTitleTotalRemaining->setText("Total remaining");
    ui->labelTitleTotalRemaining->setProperty("cssClass", "text-title");

    ui->labelAmountRemaining->setProperty("cssClass", "text-body1");

    // Refresh view
    refreshView();

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
    connect(ui->pushLeft, &QPushButton::clicked, [this](){onPIVSelected(true);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onPIVSelected(false);});
    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onSendClicked()));

    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
    connect(ui->pushButtonAddRecipient, SIGNAL(clicked()), this, SLOT(onAddEntryClicked()));
    connect(ui->pushButtonClear, SIGNAL(clicked()), this, SLOT(clearAll()));
}

void SendWidget::refreshView(){
    QString btnText;
    if(ui->pushLeft->isChecked()){
        btnText = tr("Send PIV");
    }else{
        btnText = tr("Send zPIV");
    }
    ui->pushButtonSave->setText(btnText);
    ui->labelAmountRemaining->setText("1000 zPIV");
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
                entry->setModel(model);
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

void SendWidget::clearAll(){
    coinControl->SetNull();
    clearEntries();
}

void SendWidget::clearEntries(){
    int num = entries.length();
    for (int i = 0; i < num; ++i) {
        ui->scrollAreaWidgetContents->layout()->takeAt(0)->widget()->deleteLater();
    }
    entries.clear();

    addEntry();
}

void SendWidget::addEntry(){
    if(entries.isEmpty()){
        createEntry();
    } else {
        if (entries.length() == 1) {
            SendMultiRow *entry = entries.at(0);
            entry->hideLabels();
            entry->setNumber(1);
        }else if(entries.length() == MAX_SEND_POPUP_ENTRIES){
            // TODO: Snackbar notifying that it surpassed the max amount of entries
            emit message("", tr("Maximum amount of outputs reached"), CClientUIInterface::MSG_INFORMATION_SNACK);
            return;
        }

        SendMultiRow *sendMultiRow = createEntry();
        sendMultiRow->setNumber(entries.length());
        sendMultiRow->hideLabels();
    }
}

SendMultiRow* SendWidget::createEntry(){
    SendMultiRow *sendMultiRow = new SendMultiRow(this);
    if(this->walletModel)sendMultiRow->setModel(this->walletModel);
    entries.append(sendMultiRow);
    ui->scrollAreaWidgetContents->layout()->addWidget(sendMultiRow);
    connect(sendMultiRow, &SendMultiRow::onContactsClicked, this, &SendWidget::onContactsClicked);
    return sendMultiRow;
}

void SendWidget::onAddEntryClicked(){
    // TODO: Validations here..
    addEntry();
}

void SendWidget::resizeEvent(QResizeEvent *event)
 {
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
            emit message("", tr("Invalid entry"),CClientUIInterface::MSG_INFORMATION_SNACK);
            return;
        }

    }

    if (recipients.isEmpty()) {
        //todo: notificate user about this.
        emit message("", tr("No set recipients"),CClientUIInterface::MSG_INFORMATION_SNACK);
        return;
    }

    bool sendPiv = ui->pushLeft->isChecked();

    // request unlock only if was locked or unlocked for mixing:
    // this way we let users unlock by walletpassphrase or by menu
    // and make many transactions while unlocking through this dialog
    // will call relock
    if(!GUIUtil::requestUnlock(walletModel, sendPiv ? AskPassphraseDialog::Context::Send_PIV : AskPassphraseDialog::Context::Send_zPIV, true)){
        // Unlock wallet was cancelled
        //TODO: Check what is this --> fNewRecipientAllowed = true;
        // TODO: Notify the user..
        emit message("", tr("Cannot send, wallet locked"),CClientUIInterface::MSG_INFORMATION_SNACK);
        return;
    }

    if((sendPiv) ? send(recipients) : sendZpiv(recipients)) {
        updateEntryLabels(recipients);
    }


}

bool SendWidget::send(QList<SendCoinsRecipient> recipients){
    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // TODO: Coin control
    //if (model->getOptionsModel()->getCoinControlFeatures()) // coin control enabled
    //    prepareStatus = model->prepareTransaction(currentTransaction, CoinControlDialog::coinControl);
    //else
    prepareStatus = walletModel->prepareTransaction(currentTransaction, coinControl);


    // process prepareStatus and on error generate message shown to user
    processSendCoinsReturn(prepareStatus,
                           BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                        currentTransaction.getTransactionFee()),
                           true
    );

    if (prepareStatus.status != WalletModel::OK) {
        // TODO: Check why this??
        //fNewRecipientAllowed = true;
        emit message("", tr("Prepare status failed.."),CClientUIInterface::MSG_INFORMATION_SNACK);
        return false;
    }

    CAmount txFee = currentTransaction.getTransactionFee();
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;

    window->showHide(true);
    SendConfirmDialog* dialog = new SendConfirmDialog(window);
    dialog->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setData(walletModel, currentTransaction);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if(dialog->isConfirm()){
        // now send the prepared transaction
        WalletModel::SendCoinsReturn sendStatus = dialog->getStatus();
        // process sendStatus and on error generate message shown to user
        processSendCoinsReturn(sendStatus);

        // TODO: Update
        if (sendStatus.status == WalletModel::OK) {
            //CoinControlDialog::coinControl->UnSelectAll();
            //coinControlUpdateLabels();
            //
            clearAll();
            emit message("", tr("Transaction sent"),CClientUIInterface::MSG_INFORMATION_SNACK);
            return true;
        }

    }

    dialog->deleteLater();
    return false;
}

bool SendWidget::sendZpiv(QList<SendCoinsRecipient> recipients){
    if (!walletModel || !walletModel->getOptionsModel())
        return false;

    if(GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE)) {
        emit message(tr("Spend Zerocoin"), tr("zPIV is currently undergoing maintenance."), CClientUIInterface::MSG_ERROR);
        return false;
    }

    std::list<std::pair<CBitcoinAddress*, CAmount>> outputs;
    CAmount total = 0;
    for (SendCoinsRecipient rec : recipients){
        total += rec.amount;
        outputs.push_back(std::pair<CBitcoinAddress*, CAmount>(new CBitcoinAddress(rec.address.toStdString()),rec.amount));
    }

    // TODO: add confirm dialog..
    QString sendBody = outputs.size() == 1 ?
            tr("Sending %1 to address %2\n")
            .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), total, false, BitcoinUnits::separatorAlways))
            .arg(recipients.first().address)
            :
           tr("Sending %1 to addresses\n%2")
           .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), total, false, BitcoinUnits::separatorAlways))
           .arg(recipientsToString(recipients));

    bool ret = false;
    //emit message( TODO: add this emit..
    window->message(
            tr("Spend Zerocoin"),
            sendBody,
            CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL,
            &ret);

    if(!ret) return false;

    /**
     * TODO:
     * vector<CZerocoinMint> &vMintsSelected,
            bool fMintChange,
            bool fMinimizeChange,
            CZerocoinSpendReceipt &receipt,
            std::list<std::pair<CBitcoinAddress*, CAmount>> outputs,
            std::string changeAddress = ""
     */
    vector <CZerocoinMint> vMintsSelected;
    CZerocoinSpendReceipt receipt;
    // TODO: Complete me..

    if (walletModel->sendZpiv(
            vMintsSelected,
            true,
            true,
            receipt,
            outputs
    )
            ) {
        emit message("", tr("zPIV transaction sent!"), CClientUIInterface::MSG_INFORMATION_SNACK);
        clearAll();
        return true;
    } else {
        // TODO: Detail error on the receipt..
        QString body;
        if (receipt.GetStatus() == ZPIV_SPEND_V1_SEC_LEVEL) {
            body = tr("Version 1 zPIV require a security level of 100 to successfully spend.");
        } else {
            int nNeededSpends = receipt.GetNeededSpends(); // Number of spends we would need for this transaction
            const int nMaxSpends = Params().Zerocoin_MaxSpendsPerTransaction(); // Maximum possible spends for one zPIV transaction
            if (nNeededSpends > nMaxSpends) {
                body = tr("Too much inputs (") + QString::number(nNeededSpends, 10) +
                       tr(") needed.\nMaximum allowed: ") + QString::number(nMaxSpends, 10);
                body += tr(
                        "\nEither mint higher denominations (so fewer inputs are needed) or reduce the amount to spend.");
            } else {
                body = QString::fromStdString(receipt.GetStatusMessage());
            }
        }
        emit message("zPIV transaction failed", body, CClientUIInterface::MSG_ERROR);
        return false;
    }
}

QString SendWidget::recipientsToString(QList<SendCoinsRecipient> recipients){
    QString s = "";
    for (SendCoinsRecipient rec : recipients){
        s += rec.address + " -> " + BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), rec.amount, false, BitcoinUnits::separatorAlways) + "\n";
    }
    return s;
}

void SendWidget::updateEntryLabels(QList<SendCoinsRecipient> recipients){
    for (SendCoinsRecipient rec : recipients){
        QString label = rec.label;
        if(!label.isNull()) {
            QString labelOld = walletModel->getAddressTableModel()->labelForAddress(rec.address);
            if(label.compare(labelOld) != 0) {
                CTxDestination dest = CBitcoinAddress(rec.address.toStdString()).Get();
                if (!walletModel->updateAddressBookLabels(dest, label.toStdString(),
                                                          this->walletModel->isMine(dest) ? "receive" : "send")) {
                    // Label update failed
                    emit message("", tr("Address label update failed for address: %1").arg(rec.address), CClientUIInterface::MSG_ERROR);
                    return;
                }
            }
        }

    }
}


void SendWidget::processSendCoinsReturn(const WalletModel::SendCoinsReturn& sendCoinsReturn, const QString& msgArg, bool fPrepare)
{
    bool fAskForUnlock = false;

    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch (sendCoinsReturn.status) {
        case WalletModel::InvalidAddress:
            msgParams.first = tr("The recipient address is not valid, please recheck.");
            break;
        case WalletModel::InvalidAmount:
            msgParams.first = tr("The amount to pay must be larger than 0.");
            break;
        case WalletModel::AmountExceedsBalance:
            msgParams.first = tr("The amount exceeds your balance.");
            break;
        case WalletModel::AmountWithFeeExceedsBalance:
            msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
            break;
        case WalletModel::DuplicateAddress:
            msgParams.first = tr("Duplicate address found, can only send to each address once per send operation.");
            break;
        case WalletModel::TransactionCreationFailed:
            msgParams.first = tr("Transaction creation failed!");
            msgParams.second = CClientUIInterface::MSG_ERROR;
            break;
        case WalletModel::TransactionCommitFailed:
            msgParams.first = tr("The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
            msgParams.second = CClientUIInterface::MSG_ERROR;
            break;
        case WalletModel::AnonymizeOnlyUnlocked:
            // Unlock is only need when the coins are send
            if(!fPrepare)
                fAskForUnlock = true;
            else
                msgParams.first = tr("Error: The wallet was unlocked only to anonymize coins.");
            break;

        case WalletModel::InsaneFee:
            msgParams.first = tr("A fee %1 times higher than %2 per kB is considered an insanely high fee.").arg(10000).arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), ::minRelayTxFee.GetFeePerK()));
            break;
            // included to prevent a compiler warning.
        case WalletModel::OK:
        default:
            return;
    }

    // Unlock wallet if it wasn't fully unlocked already
    if(fAskForUnlock) {
        walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full, false);
        if(walletModel->getEncryptionStatus () != WalletModel::Unlocked) {
            msgParams.first = tr("Error: The wallet was unlocked only to anonymize coins. Unlock canceled.");
        }
        else {
            // Wallet unlocked
            return;
        }
    }

    emit message(tr("Send Coins"), msgParams.first, msgParams.second);
}


void SendWidget::onChangeAddressClicked(){
    window->showHide(true);
    SendChangeAddressDialog* dialog = new SendChangeAddressDialog(window);
    if(openDialogWithOpaqueBackgroundY(dialog, window, 3, 5)) {
        if(dialog->selected) {
            QString ret;
            if (dialog->getAddress(walletModel, &ret)) {
                coinControl->destChange = CBitcoinAddress(ret.toStdString()).Get();
            }else{
                emit message("", tr("Invalid change address"), CClientUIInterface::MSG_INFORMATION_SNACK);
            }
        }
    }
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

void SendWidget::onPIVSelected(bool _isPIV){
    isPIV = _isPIV;
    coinIcon->setProperty("cssClass", _isPIV ? "coin-icon-piv" : "coin-icon-zpiv");
    refreshView();
    updateStyle(coinIcon);
}

void SendWidget::onContactsClicked(SendMultiRow* entry){
    focusedEntry = entry;
    int height = entry->getEditHeight() * 8;
    int width = entry->getEditWidth();

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this
                    );
        menuContacts->setWalletModel(walletModel);
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label){
            if(focusedEntry){
                focusedEntry->setLabel(label);
                focusedEntry->setAddress(address);
            }
        });

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

    //QPoint pos = this->mapToGlobal(entry->getEditLineRect().bottomLeft());
    QPoint pos = focusedEntry->getEditLineRect().bottomLeft();
    int posYmov = focusedEntry->getNumber() == 0 ? 1 : focusedEntry->getNumber();
    // TODO: Change this position..
    pos.setX(pos.x() + 20);
    pos.setY( (pos.y() + (focusedEntry->getEditHeight() - 4)  * 3));
    menuContacts->move(pos);
    menuContacts->show();
}

void SendWidget::resizeMenu(){
    if(menuContacts && menuContacts->isVisible() && focusedEntry){
        int width = focusedEntry->getEditWidth();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QPoint pos = focusedEntry->getEditLineRect().bottomLeft();
        // TODO: Change this position..
        pos.setX(pos.x() + 20);
        pos.setY(pos.y() + ((focusedEntry->getEditHeight() - 4)  * 3));
        menuContacts->move(pos);
    }
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
