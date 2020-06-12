// Copyright (c) 2019-2020 The PIVX developers
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
    setCssTitleScreen(ui->labelTitle);

    //Button Group
    setCssProperty(ui->pushLeft, "btn-check-left");
    setCssProperty(ui->pushRight, "btn-check-right");
    ui->pushLeft->setChecked(true);

    // Subtitle
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Key
    setCssProperty(ui->labelSubtitleKey, "text-title");
    initCssEditLine(ui->lineEditKey);

    // Passphrase
    setCssProperty(ui->labelSubtitlePassphrase, "text-title");
    initCssEditLine(ui->lineEditPassphrase);

    // Decrypt controls
    setCssProperty(ui->labelSubtitleDecryptResult, "text-title");
    ui->lineEditDecryptResult->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDecryptResult->setReadOnly(true);
    initCssEditLine(ui->lineEditDecryptResult);

    // Buttons
    setCssBtnPrimary(ui->pushButtonDecrypt);
    setCssProperty(ui->pushButtonImport, "btn-text-primary");
    ui->pushButtonImport->setVisible(false);

    connect(ui->pushLeft, &QPushButton::clicked, [this](){onEncryptSelected(true);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onEncryptSelected(false);});


    // Encrypt

    // Address
    setCssProperty(ui->labelSubtitleAddress, "text-title");
    setCssProperty(ui->addressIn_ENC, "edit-primary-multi-book");
    ui->addressIn_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->addressIn_ENC);

    // Message
    setCssProperty(ui->labelSubtitleMessage, "text-title");
    setCssProperty(ui->passphraseIn_ENC, "edit-primary");
    setShadow(ui->passphraseIn_ENC);
    ui->passphraseIn_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);

    // Encrypted Key
    setCssProperty(ui->labelSubtitleEncryptedKey, "text-title");
    ui->encryptedKeyOut_ENC->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->encryptedKeyOut_ENC->setReadOnly(true);
    initCssEditLine(ui->encryptedKeyOut_ENC);

    btnContact = ui->addressIn_ENC->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    setCssBtnPrimary(ui->pushButtonEncrypt);
    setCssBtnSecondary(ui->pushButtonClear);
    setCssBtnSecondary(ui->pushButtonDecryptClear);

    ui->statusLabel_ENC->setStyleSheet("QLabel { color: transparent; }");
    ui->statusLabel_DEC->setStyleSheet("QLabel { color: transparent; }");

    connect(ui->pushButtonEncrypt, &QPushButton::clicked, this, &SettingsBitToolWidget::onEncryptKeyButtonENCClicked);
    connect(ui->pushButtonDecrypt, &QPushButton::clicked, this, &SettingsBitToolWidget::onDecryptClicked);
    connect(ui->pushButtonImport, &QPushButton::clicked, this, &SettingsBitToolWidget::importAddressFromDecKey);
    connect(btnContact, &QAction::triggered, this, &SettingsBitToolWidget::onAddressesClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, this, &SettingsBitToolWidget::onClearAll);
    connect(ui->pushButtonDecryptClear, &QPushButton::clicked, this, &SettingsBitToolWidget::onClearDecrypt);
}

void SettingsBitToolWidget::setAddress_ENC(const QString& address)
{
    ui->addressIn_ENC->setText(address);
    ui->passphraseIn_ENC->setFocus();
}

void SettingsBitToolWidget::onEncryptSelected(bool isEncr)
{
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

    CTxDestination dest = DecodeDestination(ui->addressIn_ENC->text().toStdString());
    if (!IsValidDestination(dest)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    CKeyID keyID = *boost::get<CKeyID>(&dest);
    if (!keyID) {
        //ui->addressIn_ENC->setValid(false);
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address does not refer to a key.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
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

    std::string encryptedKey = BIP38_Encrypt(EncodeDestination(dest), qstrPassphrase.toStdString(), key.GetPrivKey_256(), key.IsCompressed());
    ui->encryptedKeyOut_ENC->setText(QString::fromStdString(encryptedKey));

    ui->statusLabel_ENC->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_ENC->setText(QString("<nobr>") + tr("Address encrypted.") + QString("</nobr>"));
}

void SettingsBitToolWidget::onClearAll()
{
    ui->addressIn_ENC->clear();
    ui->passphraseIn_ENC->clear();
    ui->encryptedKeyOut_ENC->clear();
    ui->statusLabel_ENC->clear();
    ui->addressIn_ENC->setFocus();
}

void SettingsBitToolWidget::onAddressesClicked()
{
    int addressSize = walletModel->getAddressTableModel()->sizeRecv();
    if (addressSize == 0) {
        inform(tr("No addresses available, you can go to the receive screen and add some there!"));
        return;
    }

    int height = (addressSize <= 2) ? ui->addressIn_ENC->height() * ( 2 * (addressSize + 1 )) : ui->addressIn_ENC->height() * 4;
    int width = ui->containerAddressEnc->width();

    if (!menuContacts) {
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

    if (menuContacts->isVisible()) {
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

void SettingsBitToolWidget::resizeMenu()
{
    if (menuContacts && menuContacts->isVisible()) {
        int width = ui->containerAddress->width();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QPoint pos = ui->containerAddressEnc->rect().bottomLeft();
        pos.setY(pos.y() + ui->containerAddressEnc->height());
        pos.setX(pos.x() + 10);
        menuContacts->move(pos);
    }
}

void SettingsBitToolWidget::onClearDecrypt()
{
    ui->lineEditKey->clear();
    ui->lineEditDecryptResult->clear();
    ui->lineEditPassphrase->clear();
    ui->pushButtonImport->setVisible(false);
    key = CKey();
}

void SettingsBitToolWidget::onDecryptClicked()
{
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
    ui->lineEditDecryptResult->setText(QString::fromStdString(EncodeDestination(pubKey.GetID())));
    ui->pushButtonImport->setVisible(true);
}

void SettingsBitToolWidget::importAddressFromDecKey()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CTxDestination dest = DecodeDestination(ui->lineEditDecryptResult->text().toStdString());
    CPubKey pubkey = key.GetPubKey();

    if (!IsValidDestination(dest) || !key.IsValid() || EncodeDestination(pubkey.GetID()) != EncodeDestination(dest)) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Data Not Valid.") + QString(" ") + tr("Please try again."));
        return;
    }

    CKeyID vchAddress = pubkey.GetID();
    {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Please wait while key is imported"));

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBook(vchAddress, "", AddressBook::AddressBookPurpose::RECEIVE);

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
    ui->statusLabel_DEC->setText(tr("Successfully added private key to the wallet"));
}

void SettingsBitToolWidget::resizeEvent(QResizeEvent *event)
{
    resizeMenu();
    QWidget::resizeEvent(event);
}

SettingsBitToolWidget::~SettingsBitToolWidget()
{
    delete ui;
}
