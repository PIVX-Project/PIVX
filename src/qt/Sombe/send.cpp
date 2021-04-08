//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/send.h"
#include "qt/Sombe/forms/ui_send.h"
#include "qt/Sombe/addnewcontactdialog.h"
#include "qt/Sombe/qtutils.h"
#include "qt/Sombe/sendchangeaddressdialog.h"
#include "qt/Sombe/optionbutton.h"
#include "qt/Sombe/sendconfirmdialog.h"
#include "qt/Sombe/myaddressrow.h"
#include "qt/Sombe/guitransactionsutils.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "coincontrol.h"
#include "script/standard.h"
#include "openuridialog.h"

SendWidget::SendWidget(SombeGUI* parent) :
    PWidget(parent),
    ui(new Ui::send),
    coinIcon(new QPushButton()),
    btnContacts(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(0,20,0,20);
    setCssProperty(ui->right, "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    setCssProperty(ui->labelTitle, "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Subtitle */
    setCssProperty(ui->labelSubtitle1, "text-subtitle");

    /* Address - Amount*/
    setCssProperty({ui->labelSubtitleAddress, ui->labelSubtitleAmount}, "text-title");

    /* Buttons */
    setCssBtnSecondary(ui->pushButtonFee);
    setCssProperty(ui->pushButtonClear, "btn-secundary-clear");
    setCssProperty(ui->pushButtonAddRecipient, "btn-secundary-add");
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);

    // Coin control
    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", tr("Coin Control"));
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", tr("Select the source of the coins"));

    // Change address option
    ui->btnChangeAddress->setTitleClassAndText("btn-title-grey", tr("Change Address"));
    ui->btnChangeAddress->setSubTitleClassAndText("text-subtitle", tr("Customize the change address"));

    // Uri
    ui->btnUri->setTitleClassAndText("btn-title-grey", tr("Open URI"));
    ui->btnUri->setSubTitleClassAndText("text-subtitle", tr("Parse a payment request"));

    connect(ui->pushButtonFee, &QPushButton::clicked, this, &SendWidget::onChangeCustomFeeClicked);
    connect(ui->btnCoinControl, &OptionButton::clicked, this, &SendWidget::onCoinControlClicked);
    connect(ui->btnChangeAddress, &OptionButton::clicked, this, &SendWidget::onChangeAddressClicked);
    connect(ui->btnUri, &OptionButton::clicked, this, &SendWidget::onOpenUriClicked);
    connect(ui->pushButtonReset, &QPushButton::clicked, [this](){ onResetCustomOptions(true); });
    connect(ui->checkBoxDelegations, &QCheckBox::stateChanged, this, &SendWidget::onCheckBoxChanged);

    setCssProperty(ui->coinWidget, "container-coin-type");
    setCssProperty(ui->labelLine, "container-divider");


    // Total Send
    setCssProperty(ui->labelTitleTotalSend, "text-title");
    setCssProperty(ui->labelAmountSend, "text-body1");

    // Total Remaining
    setCssProperty(ui->labelTitleTotalRemaining, "text-title");
    setCssProperty(ui->labelAmountRemaining, "text-body1");

    // Icon Send
    ui->stackedWidget->addWidget(coinIcon);
    coinIcon->show();
    coinIcon->raise();

    setCssProperty(coinIcon, "coin-icon-SBE");

    QSize BUTTON_SIZE = QSize(24, 24);
    coinIcon->setMinimumSize(BUTTON_SIZE);
    coinIcon->setMaximumSize(BUTTON_SIZE);

    int posX = 0;
    int posY = 20;
    coinIcon->move(posX, posY);

    // Entry
    addEntry();

    // Init custom fee false (updated in loadWalletModel)
    setCustomFeeSelected(false);

    // Connect
    connect(ui->pushButtonSave, &QPushButton::clicked, this, &SendWidget::onSendClicked);
    connect(ui->pushButtonAddRecipient, &QPushButton::clicked, this, &SendWidget::onAddEntryClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, [this](){clearAll(true);});
}

void SendWidget::refreshAmounts()
{
    CAmount total = 0;
    QMutableListIterator<SendMultiRow*> it(entries);
    while (it.hasNext()) {
        SendMultiRow* entry = it.next();
        CAmount amount = entry->getAmountValue();
        if (amount > 0)
            total += amount;
    }

    nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    ui->labelAmountSend->setText(GUIUtil::formatBalance(total, nDisplayUnit));

    CAmount totalAmount = 0;
    if (CoinControlDialog::coinControl->HasSelected()) {
        // Set remaining balance to the sum of the coinControl selected inputs
        totalAmount = walletModel->getBalance(CoinControlDialog::coinControl) - total;
        ui->labelTitleTotalRemaining->setText(tr("Total remaining from the selected UTXO"));
    } else {
        // Wallet's balance
        totalAmount = walletModel->getBalance(nullptr, fDelegationsChecked) - total;
        ui->labelTitleTotalRemaining->setText(tr("Total remaining"));
    }
    ui->labelAmountRemaining->setText(
            GUIUtil::formatBalance(
                    totalAmount,
                    nDisplayUnit
                    )
    );
    // show or hide delegations checkbox if need be
    showHideCheckBoxDelegations();
}

void SendWidget::loadClientModel()
{
    if (clientModel) {
        connect(clientModel, &ClientModel::numBlocksChanged, [this](){
            if (customFeeDialog) customFeeDialog->updateFee();
        });
    }
}

void SendWidget::loadWalletModel()
{
    if (walletModel) {
        if (walletModel->getOptionsModel()) {
            // display unit
            nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        }

        // set walletModel for entries
        for (SendMultiRow *entry : entries) {
            if (entry) {
                entry->setWalletModel(walletModel);
            }
        }

        // Restore custom fee from wallet Settings
        CAmount nCustomFee;
        if (walletModel->getWalletCustomFee(nCustomFee)) {
            setCustomFeeSelected(true, nCustomFee);
        }

        // Refresh
        refreshAmounts();

        // TODO: This only happen when the coin control features are modified in other screen, check before do this if the wallet has another screen modifying it.
        // Coin Control
        //connect(walletModel->getOptionsModel(), &OptionsModel::coinControlFeaturesChanged, [this](){});
        //ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        //coinControlUpdateLabels();
    }
}

void SendWidget::clearAll(bool fClearSettings)
{
    onResetCustomOptions(false);
    if (fClearSettings) onResetSettings();
    clearEntries();
    refreshAmounts();
}

void SendWidget::onResetSettings()
{
    if (customFeeDialog) customFeeDialog->clear();
    setCustomFeeSelected(false);
    if (walletModel) walletModel->setWalletCustomFee(false, DEFAULT_TRANSACTION_FEE);
}

void SendWidget::onResetCustomOptions(bool fRefreshAmounts)
{
    CoinControlDialog::coinControl->SetNull();
    ui->btnChangeAddress->setActive(false);
    ui->btnCoinControl->setActive(false);
    if (ui->checkBoxDelegations->isChecked()) ui->checkBoxDelegations->setChecked(false);
    if (fRefreshAmounts) {
        refreshAmounts();
    }
}

void SendWidget::clearEntries()
{
    int num = entries.length();
    for (int i = 0; i < num; ++i) {
        ui->scrollAreaWidgetContents->layout()->takeAt(0)->widget()->deleteLater();
    }
    entries.clear();

    addEntry();
}

void SendWidget::addEntry()
{
    if (entries.isEmpty()) {
        createEntry();
    } else {
        if (entries.length() == 1) {
            SendMultiRow *entry = entries.at(0);
            entry->hideLabels();
            entry->setNumber(1);
        } else if (entries.length() == MAX_SEND_POPUP_ENTRIES) {
            inform(tr("Maximum amount of outputs reached"));
            return;
        }

        SendMultiRow *sendMultiRow = createEntry();
        sendMultiRow->setNumber(entries.length());
        sendMultiRow->hideLabels();
    }
    setFocusOnLastEntry();
}

SendMultiRow* SendWidget::createEntry()
{
    SendMultiRow *sendMultiRow = new SendMultiRow(this);
    if (this->walletModel) sendMultiRow->setWalletModel(this->walletModel);
    entries.append(sendMultiRow);
    ui->scrollAreaWidgetContents->layout()->addWidget(sendMultiRow);
    connect(sendMultiRow, &SendMultiRow::onContactsClicked, this, &SendWidget::onContactsClicked);
    connect(sendMultiRow, &SendMultiRow::onMenuClicked, this, &SendWidget::onMenuClicked);
    connect(sendMultiRow, &SendMultiRow::onValueChanged, this, &SendWidget::onValueChanged);
    return sendMultiRow;
}

void SendWidget::onAddEntryClicked()
{
    // Check prev valid entries before add a new one.
    for (SendMultiRow* entry : entries) {
        if (!entry || !entry->validate()) {
            inform(tr("Invalid entry, previous entries must be valid before add a new one"));
            return;
        }
    }
    addEntry();
}

void SendWidget::resizeEvent(QResizeEvent *event)
{
    resizeMenu();
    QWidget::resizeEvent(event);
}

void SendWidget::showEvent(QShowEvent *event)
{
    // Set focus on last recipient address when Send-window is displayed
    setFocusOnLastEntry();

    // Update cached delegated balance
    CAmount cachedDelegatedBalance_new = walletModel->getDelegatedBalance();
    if (cachedDelegatedBalance != cachedDelegatedBalance_new) {
        cachedDelegatedBalance = cachedDelegatedBalance_new;
        refreshAmounts();
    }
}

void SendWidget::setFocusOnLastEntry()
{
    if (!entries.isEmpty()) entries.last()->setFocus();
}

void SendWidget::showHideCheckBoxDelegations()
{
    // Show checkbox only when there is any available owned delegation and
    // coincontrol is not selected.
    const bool isCControl = CoinControlDialog::coinControl->HasSelected();
    const bool hasDel = cachedDelegatedBalance > 0;

    const bool showCheckBox = !isCControl && hasDel;
    ui->checkBoxDelegations->setVisible(showCheckBox);
    if (showCheckBox)
        ui->checkBoxDelegations->setToolTip(
                tr("Possibly spend coins delegated for cold-staking (currently available: %1").arg(
                        GUIUtil::formatBalance(cachedDelegatedBalance, nDisplayUnit))
        );
}

void SendWidget::onSendClicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;

    for (SendMultiRow* entry : entries) {
        // TODO: Check UTXO splitter here..
        // Validate send..
        if (entry && entry->validate()) {
            recipients.append(entry->getValue());
        } else {
            inform(tr("Invalid entry"));
            return;
        }
    }

    if (recipients.isEmpty()) {
        inform(tr("No set recipients"));
        return;
    }

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot send, wallet locked"));
        return;
    }

    if (send(recipients)) {
        updateEntryLabels(recipients);
    }
    setFocusOnLastEntry();
}

bool SendWidget::send(QList<SendCoinsRecipient> recipients)
{
    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    prepareStatus = walletModel->prepareTransaction(currentTransaction, CoinControlDialog::coinControl, fDelegationsChecked);

    // process prepareStatus and on error generate message shown to user
    GuiTransactionsUtils::ProcessSendCoinsReturnAndInform(
            this,
            prepareStatus,
            walletModel,
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        inform(tr("Cannot create transaction."));
        return false;
    }

    showHideOp(true);
    const bool fStakeDelegationVoided = currentTransaction.getTransaction()->fStakeDelegationVoided;
    QString warningStr = QString();
    if (fStakeDelegationVoided)
        warningStr = tr("WARNING:\nTransaction spends a cold-stake delegation, voiding it.\n"
                     "These coins will no longer be cold-staked.");
    TxDetailDialog* dialog = new TxDetailDialog(window, true, warningStr);
    dialog->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setData(walletModel, currentTransaction);
    dialog->adjustSize();
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if (dialog->isConfirm()) {
        // now send the prepared transaction
        WalletModel::SendCoinsReturn sendStatus = dialog->getStatus();
        // process sendStatus and on error generate message shown to user
        GuiTransactionsUtils::ProcessSendCoinsReturnAndInform(
                this,
                sendStatus,
                walletModel
        );

        if (sendStatus.status == WalletModel::OK) {
            // if delegations were spent, update cachedBalance
            if (fStakeDelegationVoided)
                cachedDelegatedBalance = walletModel->getDelegatedBalance();
            clearAll(false);
            inform(tr("Transaction sent"));
            dialog->deleteLater();
            return true;
        }
    }

    dialog->deleteLater();
    return false;
}

QString SendWidget::recipientsToString(QList<SendCoinsRecipient> recipients)
{
    QString s = "";
    for (SendCoinsRecipient rec : recipients) {
        s += rec.address + " -> " + BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), rec.amount, false, BitcoinUnits::separatorAlways) + "\n";
    }
    return s;
}

void SendWidget::updateEntryLabels(QList<SendCoinsRecipient> recipients)
{
    for (SendCoinsRecipient rec : recipients) {
        QString label = rec.label;
        if (!label.isNull()) {
            QString labelOld = walletModel->getAddressTableModel()->labelForAddress(rec.address);
            if (label.compare(labelOld) != 0) {
                CTxDestination dest = DecodeDestination(rec.address.toStdString());
                if (!walletModel->updateAddressBookLabels(dest, label.toStdString(),
                                                          this->walletModel->isMine(dest) ?
                                                                  AddressBook::AddressBookPurpose::RECEIVE :
                                                                  AddressBook::AddressBookPurpose::SEND)) {
                    // Label update failed
                    Q_EMIT message("", tr("Address label update failed for address: %1").arg(rec.address), CClientUIInterface::MSG_ERROR);
                    return;
                }
            }
        }

    }
}


void SendWidget::onChangeAddressClicked()
{
    showHideOp(true);
    SendChangeAddressDialog* dialog = new SendChangeAddressDialog(window, walletModel);
    if (!boost::get<CNoDestination>(&CoinControlDialog::coinControl->destChange)) {
        dialog->setAddress(QString::fromStdString(EncodeDestination(CoinControlDialog::coinControl->destChange)));
    }
    if (openDialogWithOpaqueBackgroundY(dialog, window, 3, 5)) {
        CTxDestination dest = DecodeDestination(dialog->getAddress().toStdString());
        // Ask if it's what the user really wants
        if (!walletModel->isMine(dest) &&
            !ask(tr("Warning!"), tr("The change address doesn't belong to this wallet.\n\nDo you want to continue?"))) {
            return;
        }
        CoinControlDialog::coinControl->destChange = dest;
        ui->btnChangeAddress->setActive(true);
    }
    // check if changeAddress has been reset to NoDestination (or wasn't set at all)
    if (boost::get<CNoDestination>(&CoinControlDialog::coinControl->destChange))
        ui->btnChangeAddress->setActive(false);
    dialog->deleteLater();
}

void SendWidget::onOpenUriClicked()
{
    showHideOp(true);
    OpenURIDialog *dlg = new OpenURIDialog(window);
    if (openDialogWithOpaqueBackgroundY(dlg, window, 3, 5)) {

        SendCoinsRecipient rcp;
        if (!GUIUtil::parseBitcoinURI(dlg->getURI(), &rcp)) {
            inform(tr("Invalid URI"));
            return;
        }
        if (!walletModel->validateAddress(rcp.address)) {
            inform(tr("Invalid address in URI"));
            return;
        }

        int listSize = entries.size();
        if (listSize == 1) {
            SendMultiRow *entry = entries[0];
            entry->setAddressAndLabelOrDescription(rcp.address, rcp.message);
            entry->setAmount(BitcoinUnits::format(nDisplayUnit, rcp.amount, false));
        } else {
            // Use the last one if it's invalid or add a new one
            SendMultiRow *entry = entries[listSize - 1];
            if (!entry->validate()) {
                addEntry();
                entry = entries[listSize];
            }
            entry->setAddressAndLabelOrDescription(rcp.address, rcp.message);
            entry->setAmount(BitcoinUnits::format(nDisplayUnit, rcp.amount, false));
        }
        Q_EMIT receivedURI(dlg->getURI());
    }
    dlg->deleteLater();
}

void SendWidget::onChangeCustomFeeClicked()
{
    showHideOp(true);
    if (!customFeeDialog) {
        customFeeDialog = new SendCustomFeeDialog(window, walletModel);
    }
    if (openDialogWithOpaqueBackgroundY(customFeeDialog, window, 3, 5)) {
        const CAmount& nFeePerKb = customFeeDialog->getFeeRate().GetFeePerK();
        setCustomFeeSelected(customFeeDialog->isCustomFeeChecked(), nFeePerKb);
    }
}

void SendWidget::onCoinControlClicked()
{
    if (walletModel->getBalance() > 0) {
        if (!coinControlDialog) {
            coinControlDialog = new CoinControlDialog();
            coinControlDialog->setModel(walletModel);
        } else {
            coinControlDialog->refreshDialog();
        }
        setCoinControlPayAmounts();
        coinControlDialog->exec();
        ui->btnCoinControl->setActive(CoinControlDialog::coinControl->HasSelected());
        refreshAmounts();
    } else {
        inform(tr("You don't have any %1 to select.").arg(CURRENCY_UNIT.c_str()));
    }
}

void SendWidget::setCoinControlPayAmounts()
{
    if (!coinControlDialog) return;
    coinControlDialog->clearPayAmounts();
    QMutableListIterator<SendMultiRow*> it(entries);
    while (it.hasNext()) {
        coinControlDialog->addPayAmount(it.next()->getAmountValue());
    }
}

void SendWidget::onValueChanged()
{
    refreshAmounts();
}

void SendWidget::onCheckBoxChanged()
{
    const bool checked = ui->checkBoxDelegations->isChecked();
    if (checked != fDelegationsChecked) {
        fDelegationsChecked = checked;
        refreshAmounts();
    }
}

void SendWidget::onContactsClicked(SendMultiRow* entry)
{
    focusedEntry = entry;
    if (menu && menu->isVisible()) {
        menu->hide();
    }

    int contactsSize = walletModel->getAddressTableModel()->sizeSend();
    if (contactsSize == 0) {
        inform(tr("No contacts available, you can go to the contacts screen and add some there!"));
        return;
    }

    int height = (contactsSize <= 2) ? entry->getEditHeight() * ( 2 * (contactsSize + 1 )) : entry->getEditHeight() * 6;
    int width = entry->getEditWidth();

    if (!menuContacts) {
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this
        );
        menuContacts->setWalletModel(walletModel, AddressTableModel::Send);
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label) {
            if (focusedEntry) {
                if (label != "(no label)")
                    focusedEntry->setLabel(label);
                focusedEntry->setAddress(address);
            }
        });

    }

    if (menuContacts->isVisible()) {
        menuContacts->hide();
        return;
    }

    menuContacts->resizeList(width, height);
    menuContacts->setStyleSheet(this->styleSheet());
    menuContacts->adjustSize();

    QPoint pos;
    if (entries.size() > 1) {
        pos = entry->pos();
        pos.setY((pos.y() + (focusedEntry->getEditHeight() - 12) * 4));
    } else {
        pos = focusedEntry->getEditLineRect().bottomLeft();
        pos.setY((pos.y() + (focusedEntry->getEditHeight() - 12) * 3));
    }
    pos.setX(pos.x() + 20);
    menuContacts->move(pos);
    menuContacts->show();
}

void SendWidget::onMenuClicked(SendMultiRow* entry)
{
    focusedEntry = entry;
    if (menuContacts && menuContacts->isVisible()) {
        menuContacts->hide();
    }
    QPoint pos = entry->pos();
    pos.setX(pos.x() + (entry->width() - entry->getMenuBtnWidth()));
    pos.setY(pos.y() + entry->height() + (entry->getMenuBtnWidth()));

    if (!this->menu) {
        this->menu = new TooltipMenu(window, this);
        this->menu->setCopyBtnVisible(false);
        this->menu->setEditBtnText(tr("Save contact"));
        this->menu->setMinimumSize(this->menu->width() + 30,this->menu->height());
        connect(this->menu, &TooltipMenu::message, this, &AddressesWidget::message);
        connect(this->menu, &TooltipMenu::onEditClicked, this, &SendWidget::onContactMultiClicked);
        connect(this->menu, &TooltipMenu::onDeleteClicked, this, &SendWidget::onDeleteClicked);
    } else {
        this->menu->hide();
    }
    menu->move(pos);
    menu->show();
}

void SendWidget::onContactMultiClicked()
{
    if (focusedEntry) {
        QString address = focusedEntry->getAddress();
        if (address.isEmpty()) {
            inform(tr("Address field is empty"));
            return;
        }
        if (!walletModel->validateAddress(address)) {
            inform(tr("Invalid address"));
            return;
        }
        CTxDestination SBEAdd = DecodeDestination(address.toStdString());
        if (walletModel->isMine(SBEAdd)) {
            inform(tr("Cannot store your own address as contact"));
            return;
        }

        showHideOp(true);
        AddNewContactDialog *dialog = new AddNewContactDialog(window);
        QString label = walletModel->getAddressTableModel()->labelForAddress(address);
        if (!label.isNull()) {
            dialog->setTexts(tr("Update Contact"), "Edit label for the selected address:\n%1");
            dialog->setData(address, label);
        } else {
            dialog->setTexts(tr("Create New Contact"), "Save label for the selected address:\n%1");
            dialog->setData(address, "");
        }
        openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
        if (dialog->res) {
            if (label == dialog->getLabel()) {
                return;
            }
            if (walletModel->updateAddressBookLabels(SBEAdd, dialog->getLabel().toStdString(),
                    AddressBook::AddressBookPurpose::SEND)) {
                inform(tr("New Contact Stored"));
            } else {
                inform(tr("Error Storing Contact"));
            }
        }
        dialog->deleteLater();
    }

}

void SendWidget::onDeleteClicked()
{
    if (focusedEntry) {
        focusedEntry->hide();
        focusedEntry->deleteLater();
        int entryNumber = focusedEntry->getNumber();

        // remove selected entry and update row number for the others
        QMutableListIterator<SendMultiRow*> it(entries);
        while (it.hasNext()) {
            SendMultiRow* entry = it.next();
            if (focusedEntry == entry) {
                it.remove();
            } else if (focusedEntry && entry->getNumber() > entryNumber) {
                entry->setNumber(entry->getNumber() - 1);
            }
        }

        if (entries.size() == 1) {
            SendMultiRow* sendMultiRow = QMutableListIterator<SendMultiRow*>(entries).next();
            sendMultiRow->setNumber(entries.length());
            sendMultiRow->showLabels();
        }

        focusedEntry = nullptr;

        // Update total amounts
        refreshAmounts();
        setFocusOnLastEntry();
    }
}

void SendWidget::resizeMenu()
{
    if (menuContacts && menuContacts->isVisible() && focusedEntry) {
        int width = focusedEntry->getEditWidth();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QPoint pos = focusedEntry->getEditLineRect().bottomLeft();
        pos.setX(pos.x() + 20);
        pos.setY(pos.y() + ((focusedEntry->getEditHeight() - 12)  * 3));
        menuContacts->move(pos);
    }
}

void SendWidget::setCustomFeeSelected(bool isSelected, const CAmount& customFee)
{
    isCustomFeeSelected = isSelected;
    ui->pushButtonFee->setText(isCustomFeeSelected ?
                    tr("Custom Fee %1").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, customFee) + "/kB") :
                    tr("Customize Fee"));
    if (walletModel)
        walletModel->setWalletDefaultFee(customFee);
}

void SendWidget::changeTheme(bool isLightTheme, QString& theme)
{
    if (coinControlDialog) coinControlDialog->setStyleSheet(theme);
}

SendWidget::~SendWidget()
{
    delete ui;
}
