// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/masternodewizarddialog.h"
#include "qt/pivx/forms/ui_masternodewizarddialog.h"
#include "qt/pivx/qtutils.h"
#include "optionsmodel.h"
#include "activemasternode.h"
#include <QFile>
#include <QIntValidator>
#include <QRegExpValidator>

MasterNodeWizardDialog::MasterNodeWizardDialog(WalletModel *model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MasterNodeWizardDialog),
    icConfirm1(new QPushButton()),
    icConfirm3(new QPushButton()),
    icConfirm4(new QPushButton()),
    walletModel(model)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    ui->frame->setContentsMargins(10,10,10,10);

    setCssProperty({ui->labelLine1, ui->labelLine3}, "line-purple");
    setCssProperty({ui->groupBoxName, ui->groupContainer}, "container-border");
    setCssProperty({ui->pushNumber1, ui->pushNumber3, ui->pushNumber4}, "btn-number-check");
    setCssProperty({ui->pushName1, ui->pushName3, ui->pushName4}, "btn-name-check");

    ui->pushNumber1->setEnabled(false);
    ui->pushNumber3->setEnabled(false);
    ui->pushNumber4->setEnabled(false);
    ui->pushName1->setEnabled(false);
    ui->pushName3->setEnabled(false);
    ui->pushName4->setEnabled(false);

    // Frame 1
    setCssProperty(ui->labelTitle1, "text-title-dialog");
    setCssProperty(ui->labelMessage1a, "text-main-grey");
    setCssProperty(ui->labelMessage1b, "text-main-purple");

    // Frame 3
    setCssProperty(ui->labelTitle3, "text-title-dialog");
    setCssProperty(ui->labelMessage3, "text-main-grey");

    ui->lineEditName->setPlaceholderText(tr("e.g user_masternode"));
    initCssEditLine(ui->lineEditName);
    ui->lineEditName->setValidator(new QRegExpValidator(QRegExp("^[A-Za-z0-9]+"), ui->lineEditName));

    // Frame 4
    setCssProperty(ui->labelTitle4, "text-title-dialog");
    setCssProperty({ui->labelSubtitleIp, ui->labelSubtitlePort}, "text-title");
    setCssSubtitleScreen(ui->labelSubtitleAddressIp);

    ui->lineEditIpAddress->setPlaceholderText("e.g 18.255.255.255");
    ui->lineEditPort->setPlaceholderText("e.g 51472");
    initCssEditLine(ui->lineEditIpAddress);
    initCssEditLine(ui->lineEditPort);
    ui->stackedWidget->setCurrentIndex(pos);
    ui->lineEditPort->setValidator(new QIntValidator(0, 9999999, ui->lineEditPort));
    if(walletModel->isTestnet()){
        ui->lineEditPort->setEnabled(false);
        ui->lineEditPort->setText("51474");
    } else {
        ui->lineEditPort->setText("51472");
    }

    // Confirm icons
    ui->stackedIcon1->addWidget(icConfirm1);
    ui->stackedIcon3->addWidget(icConfirm3);
    ui->stackedIcon4->addWidget(icConfirm4);
    initBtn({icConfirm1, icConfirm3, icConfirm4});
    setCssProperty({icConfirm1, icConfirm3, icConfirm4}, "ic-step-confirm");

    // Connect btns
    setCssBtnPrimary(ui->btnNext);
    ui->btnNext->setText(tr("NEXT"));
    setCssProperty(ui->btnBack , "btn-dialog-cancel");
    ui->btnBack->setVisible(false);
    ui->btnBack->setText(tr("BACK"));
    setCssProperty(ui->pushButtonSkip, "ic-close");

    connect(ui->pushButtonSkip, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnNext, SIGNAL(clicked()), this, SLOT(onNextClicked()));
    connect(ui->btnBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
}

void MasterNodeWizardDialog::onNextClicked(){
    switch(pos){
        case 0:{
            ui->stackedWidget->setCurrentIndex(1);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(true);
            ui->pushNumber3->setChecked(true);

            ui->btnBack->setVisible(true);
            break;
        }
        case 1:{

            // No empty names accepted.
            if (ui->lineEditName->text().isEmpty()) {
                setCssEditLine(ui->lineEditName, false, true);
                return;
            }
            setCssEditLine(ui->lineEditName, true, true);

            ui->stackedWidget->setCurrentIndex(2);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm3->setVisible(true);
            ui->pushNumber4->setChecked(true);
            ui->btnBack->setVisible(true);
            break;
        }
        case 2:{

            // No empty address accepted
            if (ui->lineEditIpAddress->text().isEmpty()) {
                return;
            }

            icConfirm4->setVisible(true);
            ui->btnBack->setVisible(true);
            ui->btnBack->setVisible(true);
            isOk = createMN();
            accept();
        }
    }
    pos++;
}

bool MasterNodeWizardDialog::createMN(){
    if (walletModel) {
        /**
         *
        1) generate the mn key.
        2) create the mn address.
        3) send a tx with 10k to that address.
        4) get thereate output.
        5) use those values on the masternode.conf
         */

        // First create the mn key
        CKey secret;
        secret.MakeNewKey(false);
        CBitcoinSecret mnKey = CBitcoinSecret(secret);
        std::string mnKeyString = mnKey.ToString();

        // second create mn address
        QString addressLabel = ui->lineEditName->text();
        if (addressLabel.isEmpty()) {
            returnStr = tr("address label cannot be empty");
            return false;
        }
        std::string alias = addressLabel.toStdString();

        QString addressStr = ui->lineEditIpAddress->text();
        QString portStr = ui->lineEditPort->text();
        if (addressStr.isEmpty() || portStr.isEmpty()) {
            returnStr = tr("IP or port cannot be empty");
            return false;
        }
        // TODO: Validate IP address..
        int portInt = portStr.toInt();
        if (portInt <= 0 && portInt > 999999){
            returnStr = tr("Invalid port number");
            return false;
        }
        // ip + port
        std::string ipAddress = addressStr.toStdString();
        std::string port = portStr.toStdString();

        // New receive address
        CBitcoinAddress address = walletModel->getNewAddress(alias);

        // const QString& addr, const QString& label, const CAmount& amount, const QString& message
        SendCoinsRecipient sendCoinsRecipient(QString::fromStdString(address.ToString()), "", CAmount(10000) * COIN, "");

        // Send the 10 tx to one of your address
        QList<SendCoinsRecipient> recipients;
        recipients.append(sendCoinsRecipient);
        WalletModelTransaction currentTransaction(recipients);
        WalletModel::SendCoinsReturn prepareStatus;

        prepareStatus = walletModel->prepareTransaction(currentTransaction);

        // process prepareStatus and on error generate message shown to user
        processSendCoinsReturn(prepareStatus,
                               BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                            currentTransaction.getTransactionFee()),
                               true
        );

        if (prepareStatus.status != WalletModel::OK) {
            returnStr = tr("Prepare master node failed..");
            return false;
        }

        WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
        // process sendStatus and on error generate message shown to user
        processSendCoinsReturn(sendStatus);

        if (sendStatus.status == WalletModel::OK) {
            // now change the conf
            std::string strConfFile = "masternode.conf";
            std::string strDataDir = GetDataDir().string();
            if (strConfFile != boost::filesystem::basename(strConfFile) + boost::filesystem::extension(strConfFile)){
                throw std::runtime_error(strprintf(_("masternode.conf %s resides outside data directory %s"), strConfFile, strDataDir));
            }

            boost::filesystem::path pathBootstrap = GetDataDir() / strConfFile;
            if (boost::filesystem::exists(pathBootstrap)) {
                boost::filesystem::path pathMasternodeConfigFile = GetMasternodeConfigFile();
                boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);

                if (!streamConfig.good()) {
                    returnStr = tr("Invalid masternode.conf file");
                    return false;
                }

                int linenumber = 1;
                std::string lineCopy = "";
                for (std::string line; std::getline(streamConfig, line); linenumber++) {
                    if (line.empty()) continue;

                    std::istringstream iss(line);
                    std::string comment, alias, ip, privKey, txHash, outputIndex;

                    if (iss >> comment) {
                        if (comment.at(0) == '#') continue;
                        iss.str(line);
                        iss.clear();
                    }

                    if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                        iss.str(line);
                        iss.clear();
                        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                            streamConfig.close();
                            returnStr = tr("Error parsing masternode.conf file");
                            return false;
                        }
                    }
                    lineCopy += line + "\n";
                }

                if (lineCopy.size() == 0) {
                    lineCopy = "# Masternode config file\n"
                               "# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index\n"
                               "# Example: mn1 127.0.0.2:51472 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0"
                               "#";
                }
                lineCopy += "\n";

                streamConfig.close();

                CWalletTx* walletTx = currentTransaction.getTransaction();
                std::string txID = walletTx->GetHash().GetHex();
                int indexOut = -1;
                for (int i=0; i < (int)walletTx->vout.size(); i++){
                    CTxOut& out = walletTx->vout[i];
                    if (out.nValue == 10000 * COIN){
                        indexOut = i;
                    }
                }
                if (indexOut == -1) {
                    returnStr = tr("Invalid collaterall output index");
                    return false;
                }
                std::string indexOutStr = std::to_string(indexOut);

                boost::filesystem::path pathConfigFile("masternode_temp.conf");
                if (!pathConfigFile.is_complete()) pathConfigFile = GetDataDir() / pathConfigFile;
                FILE* configFile = fopen(pathConfigFile.string().c_str(), "w");
                lineCopy += alias+" "+ipAddress+":"+port+" "+mnKeyString+" "+txID+" "+indexOutStr+"\n";
                fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
                fclose(configFile);

                boost::filesystem::path pathOldConfFile("old_masternode.conf");
                if (!pathOldConfFile.is_complete()) pathOldConfFile = GetDataDir() / pathOldConfFile;
                if (boost::filesystem::exists(pathOldConfFile)) {
                    boost::filesystem::remove(pathOldConfFile);
                }
                rename(pathMasternodeConfigFile, pathOldConfFile);

                boost::filesystem::path pathNewConfFile("masternode.conf");
                if (!pathNewConfFile.is_complete()) pathNewConfFile = GetDataDir() / pathNewConfFile;
                rename(pathConfigFile, pathNewConfFile);

                mnEntry = masternodeConfig.add(alias, ipAddress+":"+port, mnKeyString, txID, indexOutStr);

                returnStr = tr("Master node created!");
                return true;
            } else{
                returnStr = tr("masternode.conf file doesn't exists");
            }
        }
    }
    return false;
}

void MasterNodeWizardDialog::onBackClicked(){
    if (pos == 0) return;
    pos--;
    switch(pos){
        case 0:{
            ui->stackedWidget->setCurrentIndex(0);
            ui->pushNumber1->setChecked(true);
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(false);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(false);
            ui->btnBack->setVisible(false);
            break;
        }
        case 1:{
            ui->stackedWidget->setCurrentIndex(1);
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(true);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            icConfirm3->setVisible(false);

            break;
        }
    }
}

void MasterNodeWizardDialog::processSendCoinsReturn(const WalletModel::SendCoinsReturn& sendCoinsReturn, const QString& msgArg, bool fPrepare)
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

    inform(msgParams.first);
}

void MasterNodeWizardDialog::inform(QString text){
    if (!snackBar)
        snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

QSize BUTTON_SIZE = QSize(22, 22);
void MasterNodeWizardDialog::initBtn(std::initializer_list<QPushButton*> args){
    for (QPushButton* btn : args) {
        btn->setMinimumSize(BUTTON_SIZE);
        btn->setMaximumSize(BUTTON_SIZE);
        btn->move(0, 0);
        btn->show();
        btn->raise();
        btn->setVisible(false);
    }
}

MasterNodeWizardDialog::~MasterNodeWizardDialog(){
    if(snackBar) delete snackBar;
    delete ui;
}
