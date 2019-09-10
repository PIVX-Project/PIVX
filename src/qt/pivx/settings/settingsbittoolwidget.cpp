// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsbittoolwidget.h"
#include "qt/pivx/settings/forms/ui_settingsbittoolwidget.h"
#include "qt/pivx/qtutils.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "bip38.h"
#include "init.h"
#include "wallet/wallet.h"
#include "askpassphrasedialog.h"

#include <string>
#include <vector>


SettingsBitToolWidget::SettingsBitToolWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBitToolWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(10,10,10,10);

    /* Title */
    ui->labelTitle->setText(tr("BIP38 Tool"));
    setCssTitleScreen(ui->labelTitle);

    //Button Group
    ui->pushLeft->setText(tr("Encrypt"));
    setCssProperty(ui->pushLeft, "btn-check-left");
    ui->pushRight->setText(tr("Decrypt"));
    setCssProperty(ui->pushRight, "btn-check-right");
    ui->pushLeft->setChecked(true);

    // Subtitle
    ui->labelSubtitle1->setText("Encrypt your PIVX addresses (key pair actually) using BIP38 encryption.\nUsing this mechanism you can share your keys without middle-man risk, only need to store your passphrase safely.");
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Key
    ui->labelSubtitleKey->setText(tr("Encrypted key"));
    setCssProperty(ui->labelSubtitleKey, "text-title");
    ui->lineEditKey->setPlaceholderText(tr("Enter a encrypted key"));
    initCssEditLine(ui->lineEditKey);

    // Passphrase
    ui->labelSubtitlePassphrase->setText(tr("Passphrase"));
    setCssProperty(ui->labelSubtitlePassphrase, "text-title");

    ui->lineEditPassphrase->setPlaceholderText(tr("Enter a passphrase "));
    initCssEditLine(ui->lineEditPassphrase);

    // Decrypt controls
    ui->labelSubtitleDecryptResult->setText(tr("Decrypted address result"));
    setCssProperty(ui->labelSubtitleDecryptResult, "text-title");
    ui->lineEditDecryptResult->setPlaceholderText(tr("Decrypted Address"));
    ui->lineEditDecryptResult->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDecryptResult->setReadOnly(true);
    initCssEditLine(ui->lineEditDecryptResult);

    // Buttons
    ui->pushButtonDecrypt->setText(tr("DECRYPT KEY"));
    setCssBtnPrimary(ui->pushButtonDecrypt);

    ui->pushButtonImport->setText(tr("Import Address"));
    setCssProperty(ui->pushButtonImport, "btn-text-primary");

    connect(ui->pushLeft, &QPushButton::clicked, [this](){onEncryptSelected(true);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onEncryptSelected(false);});


    // Encrypt

    // Address
    ui->labelSubtitleAddress->setText(tr("Enter a PIVX address"));
    setCssProperty(ui->labelSubtitleAddress, "text-title");

    ui->addressIn_ENC->setPlaceholderText(tr("Add address"));
    setCssProperty(ui->addressIn_ENC, "edit-primary-multi-book");
    ui->addressIn_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->addressIn_ENC);

    // Message
    ui->labelSubtitleMessage->setText(tr("Passphrase"));
    setCssProperty(ui->labelSubtitleMessage, "text-title");

    setCssProperty(ui->passphraseIn_ENC, "edit-primary");
    ui->passphraseIn_ENC->setPlaceholderText(tr("Write a message"));
    setCssProperty(ui->passphraseIn_ENC,"edit-primary");
    setShadow(ui->passphraseIn_ENC);
    ui->passphraseIn_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);

    ui->labelSubtitleEncryptedKey->setText(tr("Encrypted Key"));
    setCssProperty(ui->labelSubtitleEncryptedKey, "text-title");
    ui->encryptedKeyOut_ENC->setPlaceholderText(tr("Encrypted key"));
    ui->encryptedKeyOut_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->encryptedKeyOut_ENC->setReadOnly(true);
    initCssEditLine(ui->encryptedKeyOut_ENC);

    btnContact = ui->addressIn_ENC->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    ui->pushButtonEncrypt->setText(tr("ENCRYPT"));
    ui->pushButtonClear->setText(tr("CLEAR ALL"));
    ui->pushButtonDecryptClear->setText(tr("CLEAR"));
    setCssBtnPrimary(ui->pushButtonEncrypt);
    setCssBtnSecondary(ui->pushButtonClear);
    setCssBtnSecondary(ui->pushButtonDecryptClear);

    ui->statusLabel_ENC->setStyleSheet("QLabel { color: transparent; }");
    ui->statusLabel_DEC->setStyleSheet("QLabel { color: transparent; }");

    connect(ui->pushButtonEncrypt, &QPushButton::clicked, this, &SettingsBitToolWidget::onEncryptKeyButtonENCClicked);
    connect(ui->pushButtonDecrypt, SIGNAL(clicked()), this, SLOT(onDecryptClicked()));
    connect(ui->pushButtonImport, SIGNAL(clicked()), this, SLOT(importAddressFromDecKey()));
    connect(btnContact, SIGNAL(triggered()), this, SLOT(onAddressesClicked()));
    connect(ui->pushButtonClear, &QPushButton::clicked, this, &SettingsBitToolWidget::onClearAll);
    connect(ui->pushButtonDecryptClear, SIGNAL(clicked()), this, SLOT(onClearDecrypt()));
}

void SettingsBitToolWidget::setAddress_ENC(const QString& address){
    ui->addressIn_ENC->setText(address);
    ui->passphraseIn_ENC->setFocus();
}

void SettingsBitToolWidget::onEncryptSelected(bool isEncr) {
    ui->stackedWidget->setCurrentIndex(isEncr);
}

QString specialChar = "\"@!#$%&'()*+,-./:;<=>?`{|}~^_[]\\";
QString validChar = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" + specialChar;
bool isValidPassphrase(QString strPassphrase, QString& strInvalid)
{
    for (int i = 0; i < strPassphrase.size(); i++) {
        if (!validChar.contains(strPassphrase[i], Qt::CaseSensitive)) {
            if (QString("\"'").contains(strPassphrase[i]))
                continue;

            strInvalid = strPassphrase[i];
            return false;
        }
    }

    return true;
}

void SettingsBitToolWidget::onEncryptKeyButtonENCClicked()
{
    if (!walletModel)
        return;

    QString qstrPassphrase = ui->passphraseIn_ENC->text();
    QString strInvalid;
    if (!isValidPassphrase(qstrPassphrase, strInvalid)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered passphrase is invalid. ") + strInvalid + QString(" is not valid") + QString(" ") + tr("Allowed: 0-9,a-z,A-Z,") + specialChar);
        return;
    }

    CBitcoinAddress addr(ui->addressIn_ENC->text().toStdString());
    if (!addr.IsValid()) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    CKeyID keyID;
    if (!addr.GetKeyID(keyID)) {
        //ui->addressIn_ENC->setValid(false);
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address does not refer to a key.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::BIP_38, true));
    if (!ctx.isValid()) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CKey key;
    if (!pwalletMain->GetKey(keyID, key)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("Private key for the entered address is not available."));
        return;
    }

    std::string encryptedKey = BIP38_Encrypt(addr.ToString(), qstrPassphrase.toStdString(), key.GetPrivKey_256(), key.IsCompressed());
    ui->encryptedKeyOut_ENC->setText(QString::fromStdString(encryptedKey));

    ui->statusLabel_ENC->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_ENC->setText(QString("<nobr>") + tr("Address encrypted.") + QString("</nobr>"));
}

void SettingsBitToolWidget::onClearAll(){
    ui->addressIn_ENC->clear();
    ui->passphraseIn_ENC->clear();
    ui->encryptedKeyOut_ENC->clear();
    ui->statusLabel_ENC->clear();
    ui->addressIn_ENC->setFocus();
}

void SettingsBitToolWidget::onAddressesClicked(){
    int addressSize = walletModel->getAddressTableModel()->sizeRecv();
    if(addressSize == 0) {
        inform(tr("No addresses available, you can go to the receive screen and add some there!"));
        return;
    }

    int height = (addressSize <= 2) ? ui->addressIn_ENC->height() * ( 2 * (addressSize + 1 )) : ui->addressIn_ENC->height() * 4;
    int width = ui->containerAddressEnc->width();

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                width,
                height,
                this
        );
        menuContacts->setWalletModel(walletModel, AddressTableModel::Receive);
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label){
            setAddress_ENC(address);
        });

    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }

    menuContacts->resizeList(width, height);
    menuContacts->setStyleSheet(this->styleSheet());
    menuContacts->adjustSize();

    QPoint pos = ui->containerAddressEnc->rect().bottomLeft();
    pos.setY(pos.y() + ui->containerAddressEnc->height());
    pos.setX(pos.x() + 10);
    menuContacts->move(pos);
    menuContacts->show();
}

void SettingsBitToolWidget::resizeMenu(){
    if(menuContacts && menuContacts->isVisible()){
        int width = ui->containerAddress->width();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QPoint pos = ui->containerAddressEnc->rect().bottomLeft();
        pos.setY(pos.y() + ui->containerAddressEnc->height());
        pos.setX(pos.x() + 10);
        menuContacts->move(pos);
    }
}

void SettingsBitToolWidget::onClearDecrypt(){
    ui->lineEditKey->clear();
    ui->lineEditDecryptResult->clear();
    ui->lineEditPassphrase->clear();
    key = CKey();
}

void SettingsBitToolWidget::onDecryptClicked(){
    std::string strPassphrase = ui->lineEditPassphrase->text().toStdString();
    std::string strKey = ui->lineEditKey->text().toStdString();

    uint256 privKey;
    bool fCompressed;
    if (!BIP38_Decrypt(strPassphrase, strKey, privKey, fCompressed)) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Failed to decrypt.") + QString(" ") + tr("Please check the key and passphrase and try again."));
        return;
    }

    key.Set(privKey.begin(), privKey.end(), fCompressed);
    CPubKey pubKey = key.GetPubKey();
    CBitcoinAddress address(pubKey.GetID());
    ui->lineEditDecryptResult->setText(QString::fromStdString(address.ToString()));
}

void SettingsBitToolWidget::importAddressFromDecKey(){
    WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::BIP_38, true));
    if (!ctx.isValid()) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CBitcoinAddress address(ui->lineEditDecryptResult->text().toStdString());
    CPubKey pubkey = key.GetPubKey();

    if (!address.IsValid() || !key.IsValid() || CBitcoinAddress(pubkey.GetID()).ToString() != address.ToString()) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Data Not Valid.") + QString(" ") + tr("Please try again."));
        return;
    }

    CKeyID vchAddress = pubkey.GetID();
    {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Please wait while key is imported"));

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBook(vchAddress, "", "receive");

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress)) {
            ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
            ui->statusLabel_DEC->setText(tr("Cannot import address, key already held by the wallet"));
            return;
        }

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKeyPubKey(key, pubkey)) {
            ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
            ui->statusLabel_DEC->setText(tr("Error adding key to the wallet"));
            return;
        }

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
    }

    ui->statusLabel_DEC->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_DEC->setText(tr("Successfully added pivate key to the wallet"));
}

void SettingsBitToolWidget::resizeEvent(QResizeEvent *event){
    resizeMenu();
    QWidget::resizeEvent(event);
}

SettingsBitToolWidget::~SettingsBitToolWidget()
{
    delete ui;
}
