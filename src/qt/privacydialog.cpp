// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privacydialog.h"
#include "ui_privacydialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "coincontroldialog.h"
#include "libzerocoin/Denominations.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "walletmodel.h"
#include "coincontrol.h"
#include "zpivcontroldialog.h"
#include "spork.h"
#include "askpassphrasedialog.h"

#include <QClipboard>
#include <QSettings>
#include <utilmoneystr.h>
#include <QtWidgets>
#include <zpiv/deterministicmint.h>
#include <zpiv/accumulators.h>

PrivacyDialog::PrivacyDialog(QWidget* parent) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowCloseButtonHint),
                                                ui(new Ui::PrivacyDialog),
                                                walletModel(0),
                                                currentBalance(-1),
                                                fDenomsMinimized(true)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);

    // "Spending 999999 zPIV ought to be enough for anybody." - Bill Gates, 2017
    ui->zPIVpayAmount->setValidator( new QDoubleValidator(0.0, 21000000.0, 20, this) );
    //ui->labelMintAmountValue->setValidator( new QIntValidator(0, 999999, this) );     // disable MINT

    // Default texts for (mini-) coincontrol
    //ui->labelCoinControlQuantity->setText (tr("Coins automatically selected"));       // disable MINT
    //ui->labelCoinControlAmount->setText (tr("Coins automatically selected"));         // disable MINT
    ui->labelzPIVSyncStatus->setText("(" + tr("out of sync") + ")");

    // Sunken frame for minting messages
    ui->TEMintStatus->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    ui->TEMintStatus->setLineWidth (2);
    ui->TEMintStatus->setMidLineWidth (2);
    ui->TEMintStatus->setPlainText(tr("Mint Status: Okay"));

    // Coin Control signals
    /*                                                                            [disable MINT and coinControl]
    connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));

    // Coin Control: clipboard actions
    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    */

    // Denomination labels
    ui->labelzDenom1Text->setText(tr("Denom. with value <b>1</b>:"));
    ui->labelzDenom2Text->setText(tr("Denom. with value <b>5</b>:"));
    ui->labelzDenom3Text->setText(tr("Denom. with value <b>10</b>:"));
    ui->labelzDenom4Text->setText(tr("Denom. with value <b>50</b>:"));
    ui->labelzDenom5Text->setText(tr("Denom. with value <b>100</b>:"));
    ui->labelzDenom6Text->setText(tr("Denom. with value <b>500</b>:"));
    ui->labelzDenom7Text->setText(tr("Denom. with value <b>1000</b>:"));
    ui->labelzDenom8Text->setText(tr("Denom. with value <b>5000</b>:"));

    // AutoMint status
    ui->label_AutoMintStatus->setText(tr("AutoMint Status:"));

    // Global Supply labels
    ui->labelZsupplyText1->setText(tr("Denom. <b>1</b>:"));
    ui->labelZsupplyText5->setText(tr("Denom. <b>5</b>:"));
    ui->labelZsupplyText10->setText(tr("Denom. <b>10</b>:"));
    ui->labelZsupplyText50->setText(tr("Denom. <b>50</b>:"));
    ui->labelZsupplyText100->setText(tr("Denom. <b>100</b>:"));
    ui->labelZsupplyText500->setText(tr("Denom. <b>500</b>:"));
    ui->labelZsupplyText1000->setText(tr("Denom. <b>1000</b>:"));
    ui->labelZsupplyText5000->setText(tr("Denom. <b>5000</b>:"));

    // PIVX settings
    QSettings settings;
    if (!settings.contains("fMinimizeChange")){
        fMinimizeChange = false;
        settings.setValue("fMinimizeChange", fMinimizeChange);
    }
    else{
        fMinimizeChange = settings.value("fMinimizeChange").toBool();
    }

    ui->checkBoxMinimizeChange->setChecked(fMinimizeChange);

    // Start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    // Hide those placeholder elements needed for CoinControl interaction
    ui->WarningLabel->hide();    // Explanatory text visible in QT-Creator
    ui->dummyHideWidget->hide(); // Dummy widget with elements to hide

    // Set labels/buttons depending on SPORK_16 status
    updateSPORK16Status();

    // init Denoms section
    if(!settings.contains("fDenomsSectionMinimized"))
        settings.setValue("fDenomsSectionMinimized", true);
    minimizeDenomsSection(settings.value("fDenomsSectionMinimized").toBool());

    ui->checkBoxMintChange->setVisible(false);
}

PrivacyDialog::~PrivacyDialog()
{
    QSettings settings;
    settings.setValue("fDenomsSectionMinimized", fDenomsMinimized);
    delete ui;
}

void PrivacyDialog::setModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    if (walletModel && walletModel->getOptionsModel()) {
        // Keep up to date with wallet
        setBalance(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(),
                   walletModel->getZerocoinBalance(), walletModel->getUnconfirmedZerocoinBalance(), walletModel->getImmatureZerocoinBalance(),
                   walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance());

        connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this,
                SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        connect(walletModel->getOptionsModel(), SIGNAL(zeromintEnableChanged(bool)), this, SLOT(updateAutomintStatus()));
        connect(walletModel->getOptionsModel(), SIGNAL(zeromintPercentageChanged(int)), this, SLOT(updateAutomintStatus()));
    }
}

void PrivacyDialog::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void PrivacyDialog::on_addressBookButton_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(walletModel->getAddressTableModel());
    if (dlg.exec()) {
        ui->payTo->setText(dlg.getReturnValue());
        ui->zPIVpayAmount->setFocus();
    }
}
/* disable MINT
 *
void PrivacyDialog::on_pushButtonMintzPIV_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    if(GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE)) {
        QMessageBox::information(this, tr("Mint Zerocoin"),
                                 tr("zPIV is currently undergoing maintenance."), QMessageBox::Ok,
                                 QMessageBox::Ok);
        return;
    }

    // Reset message text
    ui->TEMintStatus->setPlainText(tr("Mint Status: Okay"));

    // Request unlock if wallet was locked or unlocked for mixing:
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Mint_zPIV, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            ui->TEMintStatus->setPlainText(tr("Error: Your wallet is locked. Please enter the wallet passphrase first."));
            return;
        }
    }

    QString sAmount = ui->labelMintAmountValue->text();
    CAmount nAmount = sAmount.toInt() * COIN;

    // Minting amount must be > 0
    if(nAmount <= 0){
        ui->TEMintStatus->setPlainText(tr("Message: Enter an amount > 0."));
        return;
    }

    ui->TEMintStatus->setPlainText(tr("Minting ") + ui->labelMintAmountValue->text() + " zPIV...");
    ui->TEMintStatus->repaint ();

    int64_t nTime = GetTimeMillis();

    CWalletTx wtx;
    std::vector<CDeterministicMint> vMints;
    std::string strError = pwalletMain->MintZerocoin(nAmount, wtx, vMints, CoinControlDialog::coinControl);

    // Return if something went wrong during minting
    if (strError != ""){
        ui->TEMintStatus->setPlainText(QString::fromStdString(strError));
        return;
    }

    double fDuration = (double)(GetTimeMillis() - nTime)/1000.0;

    // Minting successfully finished. Show some stats for entertainment.
    QString strStatsHeader = tr("Successfully minted ") + ui->labelMintAmountValue->text() + tr(" zPIV in ") +
                             QString::number(fDuration) + tr(" sec. Used denominations:\n");

    // Clear amount to avoid double spending when accidentally clicking twice
    ui->labelMintAmountValue->setText ("0");

    QString strStats = "";
    ui->TEMintStatus->setPlainText(strStatsHeader);

    for (CDeterministicMint dMint : vMints) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        strStats = strStats + QString::number(dMint.GetDenomination()) + " ";
        ui->TEMintStatus->setPlainText(strStatsHeader + strStats);
        ui->TEMintStatus->repaint ();

    }

    ui->TEMintStatus->verticalScrollBar()->setValue(ui->TEMintStatus->verticalScrollBar()->maximum()); // Automatically scroll to end of text

    // Available balance isn't always updated, so force it.
    setBalance(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(),
               walletModel->getZerocoinBalance(), walletModel->getUnconfirmedZerocoinBalance(), walletModel->getImmatureZerocoinBalance(),
               walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance());
    coinControlUpdateLabels();

    return;
}
*/
void PrivacyDialog::on_pushButtonMintReset_clicked()
{
    ui->TEMintStatus->setPlainText(tr("Starting ResetMintZerocoin: rescanning complete blockchain, this will need up to 30 minutes depending on your hardware.\nPlease be patient..."));
    ui->TEMintStatus->repaint ();

    int64_t nTime = GetTimeMillis();
    std::string strResetMintResult = pwalletMain->ResetMintZerocoin();
    double fDuration = (double)(GetTimeMillis() - nTime)/1000.0;
    ui->TEMintStatus->setPlainText(QString::fromStdString(strResetMintResult) + tr("Duration: ") + QString::number(fDuration) + tr(" sec.\n"));
    ui->TEMintStatus->repaint ();
    ui->TEMintStatus->verticalScrollBar()->setValue(ui->TEMintStatus->verticalScrollBar()->maximum()); // Automatically scroll to end of text

    return;
}


void PrivacyDialog::on_pushButtonSpentReset_clicked()
{
    ui->TEMintStatus->setPlainText(tr("Starting ResetSpentZerocoin: "));
    ui->TEMintStatus->repaint ();
    int64_t nTime = GetTimeMillis();
    std::string strResetSpentResult = pwalletMain->ResetSpentZerocoin();
    double fDuration = (double)(GetTimeMillis() - nTime)/1000.0;
    ui->TEMintStatus->setPlainText(QString::fromStdString(strResetSpentResult) + tr("Duration: ") + QString::number(fDuration) + tr(" sec.\n"));
    ui->TEMintStatus->repaint ();
    ui->TEMintStatus->verticalScrollBar()->setValue(ui->TEMintStatus->verticalScrollBar()->maximum()); // Automatically scroll to end of text

    return;
}

void PrivacyDialog::on_pushButtonSpendzPIV_clicked()
{

    if (!walletModel || !walletModel->getOptionsModel() || !pwalletMain)
        return;

    if(GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE)) {
        QMessageBox::information(this, tr("Mint Zerocoin"),
                                 tr("zPIV is currently undergoing maintenance."), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    // Request unlock if wallet was locked or unlocked for mixing:
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Send_zPIV, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            return;
        }
        // Wallet is unlocked now, sedn zPIV
        sendzPIV();
        return;
    }
    // Wallet already unlocked or not encrypted at all, send zPIV
    sendzPIV();
}

void PrivacyDialog::on_pushButtonZPivControl_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    ZPivControlDialog* zPivControl = new ZPivControlDialog(this);
    zPivControl->setModel(walletModel);
    zPivControl->exec();
}

void PrivacyDialog::setZPivControlLabels(int64_t nAmount, int nQuantity)
{
    ui->labelzPivSelected_int->setText(QString::number(nAmount));
    ui->labelQuantitySelected_int->setText(QString::number(nQuantity));
}

static inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

void PrivacyDialog::sendzPIV()
{
    QSettings settings;

    // Handle 'Pay To' address options
    CBitcoinAddress address(ui->payTo->text().toStdString());
    if(ui->payTo->text().isEmpty()){
        QMessageBox::information(this, tr("Spend Zerocoin"), tr("No 'Pay To' address provided, creating local payment"), QMessageBox::Ok, QMessageBox::Ok);
    }
    else{
        if (!address.IsValid()) {
            QMessageBox::warning(this, tr("Spend Zerocoin"), tr("Invalid Pivx Address"), QMessageBox::Ok, QMessageBox::Ok);
            ui->payTo->setFocus();
            return;
        }
    }

    // Double is allowed now
    double dAmount = ui->zPIVpayAmount->text().toDouble();
    CAmount nAmount = roundint64(dAmount* COIN);

    // Check amount validity
    if (!MoneyRange(nAmount) || nAmount <= 0.0) {
        QMessageBox::warning(this, tr("Spend Zerocoin"), tr("Invalid Send Amount"), QMessageBox::Ok, QMessageBox::Ok);
        ui->zPIVpayAmount->setFocus();
        return;
    }

    // Convert change to zPIV
    bool fMintChange = false;// ui->checkBoxMintChange->isChecked();

    // Persist minimize change setting
    fMinimizeChange = ui->checkBoxMinimizeChange->isChecked();
    settings.setValue("fMinimizeChange", fMinimizeChange);

    // Warn for additional fees if amount is not an integer and change as zPIV is requested
    bool fWholeNumber = floor(dAmount) == dAmount;
    double dzFee = 0.0;

    if(!fWholeNumber)
        dzFee = 1.0 - (dAmount - floor(dAmount));

    if(!fWholeNumber && fMintChange){
        QString strFeeWarning = "You've entered an amount with fractional digits and want the change to be converted to Zerocoin.<br /><br /><b>";
        strFeeWarning += QString::number(dzFee, 'f', 8) + " PIV </b>will be added to the standard transaction fees!<br />";
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm additional Fees"),
                                                                   strFeeWarning,
                                                                   QMessageBox::Yes | QMessageBox::Cancel,
                                                                   QMessageBox::Cancel);

        if (retval != QMessageBox::Yes) {
            // Sending canceled
            ui->zPIVpayAmount->setFocus();
            return;
        }
    }

    // Spend confirmation message box

    // Add address info if available
    QString strAddressLabel = "";
    if(!ui->payTo->text().isEmpty() && !ui->addAsLabel->text().isEmpty()){
        strAddressLabel = "<br />(" + ui->addAsLabel->text() + ") ";
    }

    // General info
    QString strQuestionString = tr("Are you sure you want to send?<br /><br />");
    QString strAmount = "<b>" + QString::number(dAmount, 'f', 8) + " zPIV</b>";
    QString strAddress = tr(" to address ") + QString::fromStdString(address.ToString()) + strAddressLabel + " <br />";

    if(ui->payTo->text().isEmpty()){
        // No address provided => send to local address
        strAddress = tr(" to a newly generated (unused and therefore anonymous) local address <br />");
    }

    strQuestionString += strAmount + strAddress;

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send coins"),
                                                               strQuestionString,
                                                               QMessageBox::Yes | QMessageBox::Cancel,
                                                               QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        // Sending canceled
        return;
    }

    int64_t nTime = GetTimeMillis();
    ui->TEMintStatus->setPlainText(tr("Spending Zerocoin.\nComputationally expensive, might need several minutes depending on your hardware.\nPlease be patient..."));
    ui->TEMintStatus->repaint();

    // use mints from zPIV selector if applicable
    std::vector<CMintMeta> vMintsToFetch;
    std::vector<CZerocoinMint> vMintsSelected;
    if (!ZPivControlDialog::setSelectedMints.empty()) {
        vMintsToFetch = ZPivControlDialog::GetSelectedMints();

        for (auto& meta : vMintsToFetch) {
            CZerocoinMint mint;
            if (!pwalletMain->GetMint(meta.hashSerial, mint)) {
                ui->TEMintStatus->setPlainText(tr("Failed to fetch mint associated with serial hash"));
                ui->TEMintStatus->repaint();
                return;
            }
            vMintsSelected.emplace_back(mint);
        }
    }

    // Spend zPIV
    CWalletTx wtxNew;
    CZerocoinSpendReceipt receipt;
    bool fSuccess = false;
    std::list<std::pair<CBitcoinAddress*, CAmount>> out;
    if(ui->payTo->text().isEmpty()){
        // Spend to newly generated local address
        fSuccess = pwalletMain->SpendZerocoin(nAmount, wtxNew, receipt, vMintsSelected, fMintChange, fMinimizeChange, out);
    }
    else {
        // Spend to supplied destination address
        fSuccess = pwalletMain->SpendZerocoin(nAmount, wtxNew, receipt, vMintsSelected, fMintChange, fMinimizeChange, out, &address);
    }

    // Display errors during spend
    if (!fSuccess) {
        /*
        int nNeededSpends = receipt.GetNeededSpends(); // Number of spends we would need for this transaction
        const int nMaxSpends = Params().Zerocoin_MaxSpendsPerTransaction(); // Maximum possible spends for one zPIV transaction
        if (nNeededSpends > nMaxSpends) {
            QString strStatusMessage = tr("Too much inputs (") + QString::number(nNeededSpends, 10) + tr(") needed.\nMaximum allowed: ") + QString::number(nMaxSpends, 10);
            strStatusMessage += tr("\nEither mint higher denominations (so fewer inputs are needed) or reduce the amount to spend.");
            QMessageBox::warning(this, tr("Spend Zerocoin"), strStatusMessage.toStdString().c_str(), QMessageBox::Ok, QMessageBox::Ok);
            ui->TEMintStatus->setPlainText(tr("Spend Zerocoin failed with status = ") +QString::number(receipt.GetStatus(), 10) + "\n" + "Message: " + QString::fromStdString(strStatusMessage.toStdString()));
        }
        else {
         */
        QMessageBox::warning(this, tr("Spend Zerocoin"), receipt.GetStatusMessage().c_str(), QMessageBox::Ok, QMessageBox::Ok);
        ui->TEMintStatus->setPlainText(tr("Spend Zerocoin failed with status = ") +QString::number(receipt.GetStatus(), 10) + "\n" + "Message: " + QString::fromStdString(receipt.GetStatusMessage()));
        //}
        ui->zPIVpayAmount->setFocus();
        ui->TEMintStatus->repaint();
        ui->TEMintStatus->verticalScrollBar()->setValue(ui->TEMintStatus->verticalScrollBar()->maximum()); // Automatically scroll to end of text
        return;
    }

    if (walletModel && walletModel->getAddressTableModel()) {
        // If zPIV was spent successfully update the addressbook with the label
        std::string labelText = ui->addAsLabel->text().toStdString();
        if (!labelText.empty())
            walletModel->updateAddressBookLabels(address.Get(), labelText, "send");
        else
            walletModel->updateAddressBookLabels(address.Get(), "(no label)", "send");
    }

    // Clear zpiv selector in case it was used
    ZPivControlDialog::setSelectedMints.clear();
    ui->labelzPivSelected_int->setText(QString("0"));
    ui->labelQuantitySelected_int->setText(QString("0"));

    // Some statistics for entertainment
    QString strStats = "";
    CAmount nValueIn = 0;
    int nCount = 0;
    for (CZerocoinSpend spend : receipt.GetSpends()) {
        strStats += tr("zPIV Spend #: ") + QString::number(nCount) + ", ";
        strStats += tr("denomination: ") + QString::number(spend.GetDenomination()) + ", ";
        strStats += tr("serial: ") + spend.GetSerial().ToString().c_str() + "\n";
        strStats += tr("Spend is 1 of : ") + QString::number(spend.GetMintCount()) + " mints in the accumulator\n";
        nValueIn += libzerocoin::ZerocoinDenominationToAmount(spend.GetDenomination());
        ++nCount;
    }

    CAmount nValueOut = 0;
    for (const CTxOut& txout: wtxNew.vout) {
        strStats += tr("value out: ") + FormatMoney(txout.nValue).c_str() + " Piv, ";
        nValueOut += txout.nValue;

        strStats += tr("address: ");
        CTxDestination dest;
        if(txout.IsZerocoinMint())
            strStats += tr("zPIV Mint");
        else if(ExtractDestination(txout.scriptPubKey, dest))
            strStats += tr(CBitcoinAddress(dest).ToString().c_str());
        strStats += "\n";
    }
    double fDuration = (double)(GetTimeMillis() - nTime)/1000.0;
    strStats += tr("Duration: ") + QString::number(fDuration) + tr(" sec.\n");
    strStats += tr("Sending successful, return code: ") + QString::number(receipt.GetStatus()) + "\n";

    QString strReturn;
    strReturn += tr("txid: ") + wtxNew.GetHash().ToString().c_str() + "\n";
    strReturn += tr("fee: ") + QString::fromStdString(FormatMoney(nValueIn-nValueOut)) + "\n";
    strReturn += strStats;

    // Clear amount to avoid double spending when accidentally clicking twice
    ui->zPIVpayAmount->setText ("0");

    ui->TEMintStatus->setPlainText(strReturn);
    ui->TEMintStatus->repaint();
    ui->TEMintStatus->verticalScrollBar()->setValue(ui->TEMintStatus->verticalScrollBar()->maximum()); // Automatically scroll to end of text
}

void PrivacyDialog::on_payTo_textChanged(const QString& address)
{
    updateLabel(address);
}

/* DISABLE MINTs: no need for coinCointrol

// Coin Control: copy label "Quantity" to clipboard
void PrivacyDialog::coinControlClipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void PrivacyDialog::coinControlClipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: button inputs -> show actual coin control dialog
void PrivacyDialog::coinControlButtonClicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    CoinControlDialog dlg;
    dlg.setModel(walletModel);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: update labels
void PrivacyDialog::coinControlUpdateLabels()
{
    if (!walletModel || !walletModel->getOptionsModel() || !walletModel->getOptionsModel()->getCoinControlFeatures())
        return;

     // set pay amounts
    CoinControlDialog::payAmounts.clear();

    if (CoinControlDialog::coinControl->HasSelected()) {
        // Actual coin control calculation
        CoinControlDialog::updateLabels(walletModel, this);
    } else {
        ui->labelCoinControlQuantity->setText (tr("Coins automatically selected"));
        ui->labelCoinControlAmount->setText (tr("Coins automatically selected"));
    }
}
*/

void PrivacyDialog::on_pushButtonShowDenoms_clicked()
{
    minimizeDenomsSection(false);
}

void PrivacyDialog::on_pushButtonHideDenoms_clicked()
{
    minimizeDenomsSection(true);
}

void PrivacyDialog::minimizeDenomsSection(bool fMinimize)
{
    if (fMinimize) {
        ui->balanceSupplyFrame->show();
        ui->verticalFrameRight->hide();
    } else {
        ui->balanceSupplyFrame->hide();
        ui->verticalFrameRight->show();
    }
    fDenomsMinimized = fMinimize;
}

bool PrivacyDialog::updateLabel(const QString& address)
{
    if (!walletModel)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = walletModel->getAddressTableModel()->labelForAddress(address);
    if (!associatedLabel.isEmpty()) {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

void PrivacyDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                               const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                               const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentZerocoinBalance = zerocoinBalance;
    currentUnconfirmedZerocoinBalance = unconfirmedZerocoinBalance;
    currentImmatureZerocoinBalance = immatureZerocoinBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;

    std::map<libzerocoin::CoinDenomination, CAmount> mapDenomBalances;
    std::map<libzerocoin::CoinDenomination, int> mapUnconfirmed;
    std::map<libzerocoin::CoinDenomination, int> mapImmature;
    for (const auto& denom : libzerocoin::zerocoinDenomList){
        mapDenomBalances.insert(std::make_pair(denom, 0));
        mapUnconfirmed.insert(std::make_pair(denom, 0));
        mapImmature.insert(std::make_pair(denom, 0));
    }

    std::vector<CMintMeta> vMints = pwalletMain->zpivTracker->GetMints(false);
    std::map<libzerocoin::CoinDenomination, int> mapMaturityHeights = GetMintMaturityHeight();
    for (auto& meta : vMints){
        // All denominations
        mapDenomBalances.at(meta.denom)++;

        if (!meta.nHeight || chainActive.Height() - meta.nHeight <= Params().Zerocoin_MintRequiredConfirmations()) {
            // All unconfirmed denominations
            mapUnconfirmed.at(meta.denom)++;
        } else {
            if (meta.denom == libzerocoin::CoinDenomination::ZQ_ERROR) {
                mapImmature.at(meta.denom)++;
            } else if (meta.nHeight >= mapMaturityHeights.at(meta.denom)) {
                mapImmature.at(meta.denom)++;
            }
        }
    }

    int64_t nCoins = 0;
    int64_t nSumPerCoin = 0;
    int64_t nUnconfirmed = 0;
    int64_t nImmature = 0;
    QString strDenomStats, strUnconfirmed = "";

    for (const auto& denom : libzerocoin::zerocoinDenomList) {
        nCoins = libzerocoin::ZerocoinDenominationToInt(denom);
        nSumPerCoin = nCoins * mapDenomBalances.at(denom);
        nUnconfirmed = mapUnconfirmed.at(denom);
        nImmature = mapImmature.at(denom);

        strUnconfirmed = "";
        if (nUnconfirmed) {
            strUnconfirmed += QString::number(nUnconfirmed) + QString(" unconf. ");
        }
        if(nImmature) {
            strUnconfirmed += QString::number(nImmature) + QString(" immature ");
        }
        if(nImmature || nUnconfirmed) {
            strUnconfirmed = QString("( ") + strUnconfirmed + QString(") ");
        }

        strDenomStats = strUnconfirmed + QString::number(mapDenomBalances.at(denom)) + " x " +
                        QString::number(nCoins) + " = <b>" +
                        QString::number(nSumPerCoin) + " zPIV </b>";

        switch (nCoins) {
            case libzerocoin::CoinDenomination::ZQ_ONE:
                ui->labelzDenom1Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                ui->labelzDenom2Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                ui->labelzDenom3Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                ui->labelzDenom4Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                ui->labelzDenom5Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                ui->labelzDenom6Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                ui->labelzDenom7Amount->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                ui->labelzDenom8Amount->setText(strDenomStats);
                break;
            default:
                // Error Case: don't update display
                break;
        }
    }
    CAmount matureZerocoinBalance = zerocoinBalance - unconfirmedZerocoinBalance - immatureZerocoinBalance;
    CAmount nLockedBalance = 0;
    if (walletModel) {
        nLockedBalance = walletModel->getLockedBalance();
    }

    ui->labelzAvailableAmount->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV "));
    ui->labelzAvailableAmount_2->setText(QString::number(matureZerocoinBalance/COIN) + QString(" zPIV "));
    ui->labelzAvailableAmount_4->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV "));

    // Display AutoMint status
    updateAutomintStatus();

    // Update/enable labels and buttons depending on the current SPORK_16 status
    updateSPORK16Status();

    // Display global supply
    ui->labelZsupplyAmount->setText(QString::number(chainActive.Tip()->GetZerocoinSupply()/COIN) + QString(" <b>zPIV </b> "));
    ui->labelZsupplyAmount_2->setText(QString::number(chainActive.Tip()->GetZerocoinSupply()/COIN) + QString(" <b>zPIV </b> "));

    for (auto denom : libzerocoin::zerocoinDenomList) {
        int64_t nSupply = chainActive.Tip()->mapZerocoinSupply.at(denom);
        QString strSupply = QString::number(nSupply) + " x " + QString::number(denom) + " = <b>" +
                            QString::number(nSupply*denom) + " zPIV </b> ";
        switch (denom) {
            case libzerocoin::CoinDenomination::ZQ_ONE:
                ui->labelZsupplyAmount1->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                ui->labelZsupplyAmount5->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                ui->labelZsupplyAmount10->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                ui->labelZsupplyAmount50->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                ui->labelZsupplyAmount100->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                ui->labelZsupplyAmount500->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                ui->labelZsupplyAmount1000->setText(strSupply);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                ui->labelZsupplyAmount5000->setText(strSupply);
                break;
            default:
                // Error Case: don't update display
                break;
        }
    }
}

void PrivacyDialog::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentZerocoinBalance, currentUnconfirmedZerocoinBalance, currentImmatureZerocoinBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);
    }
}

void PrivacyDialog::showOutOfSyncWarning(bool fShow)
{
    ui->labelzPIVSyncStatus->setVisible(fShow);
}

void PrivacyDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() != Qt::Key_Escape) // press esc -> ignore
    {
        this->QDialog::keyPressEvent(event);
    } else {
        event->ignore();
    }
}

void PrivacyDialog::updateAutomintStatus()
{
    QString strAutomintStatus = tr("AutoMint Status:");

    if (pwalletMain->isZeromintEnabled ()) {
        strAutomintStatus += tr(" <b>enabled</b>.");
    }
    else {
        strAutomintStatus += tr(" <b>disabled</b>.");
    }

    strAutomintStatus += tr(" Configured target percentage: <b>") + QString::number(pwalletMain->getZeromintPercentage()) + "%</b>";
    ui->label_AutoMintStatus->setText(strAutomintStatus);
}

void PrivacyDialog::updateSPORK16Status()
{
    // Update/enable labels, buttons and tooltips depending on the current SPORK_16 status
    //bool fButtonsEnabled =  ui->pushButtonMintzPIV->isEnabled();
    bool fButtonsEnabled = false;
    bool fMaintenanceMode = GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE);
    if (fMaintenanceMode && fButtonsEnabled) {
        // Mint zPIV
        //ui->pushButtonMintzPIV->setEnabled(false);
        //ui->pushButtonMintzPIV->setToolTip(tr("zPIV is currently disabled due to maintenance."));

        // Spend zPIV
        ui->pushButtonSpendzPIV->setEnabled(false);
        ui->pushButtonSpendzPIV->setToolTip(tr("zPIV is currently disabled due to maintenance."));
    } else if (!fMaintenanceMode && !fButtonsEnabled) {
        // Mint zPIV
        //ui->pushButtonMintzPIV->setEnabled(true);
        //ui->pushButtonMintzPIV->setToolTip(tr("PrivacyDialog", "Enter an amount of PIV to convert to zPIV", 0));

        // Spend zPIV
        ui->pushButtonSpendzPIV->setEnabled(true);
        ui->pushButtonSpendzPIV->setToolTip(tr("Spend Zerocoin. Without 'Pay To:' address creates payments to yourself."));
    }
}
