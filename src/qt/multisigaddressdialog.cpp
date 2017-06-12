// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "multisigaddressdialog.h"
#include "ui_multisigaddressdialog.h"

#include "addressbookpage.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "script/script.h"
#include "../base58.h"
#include "init.h"
#include "wallet.h"

#include <string>
#include <vector>

#include <QtCore/QVariant>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QMap>
#include <QtWidgets/QPushButton>
#include <QClipboard>
#include "qvalidatedlineedit.h"


using namespace boost;


MultiSigAddressDialog::MultiSigAddressDialog(QWidget* parent) :  QDialog(parent),
                                                                 ui(new Ui::MultiSigAddressDialog),
                                                                 model(0)
{
    ui->setupUi(this);
    ui->continueButton->installEventFilter(this);
}

MultiSigAddressDialog::~MultiSigAddressDialog()
{
    delete ui;
}

void MultiSigAddressDialog::setAddress(const QString& address)
{

}

void MultiSigAddressDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void MultiSigAddressDialog::showPage(int pageNumber)
{
    ui->stackedWidget->setCurrentIndex(pageNumber);
    this->show();
}
int m,n;
//QMap<QString, QValidatedLineEdit*> lineEdits;
QValidatedLineEdit* lineEdits[16];
void MultiSigAddressDialog::on_continueButton_clicked()
{
    if(!model)
       return;

    m = ui->enterMSpinbox->value();
    n = ui->enterNSpinbox->value();

    if(m > n) {
        ui->continueStatus->setStyleSheet("QLabel { color: red; }");
        ui->continueStatus->setText("The amount of required signatures must be less than or equal to the total number of addresses.");
    } else {
        for(int i = 0; i < n; i++){
            std::stringstream ss;
            /*Create address line layout*/
            QHBoxLayout *address = new QHBoxLayout();
            address->setSpacing(0);
            ss << "address_" << i;
            address->setObjectName(QString::fromStdString(ss.str()));
            ss.str(std::string());

            /*create address label*/
            QLabel *addressLabel = new QLabel(ui->addressList);
            ss << "addressLabel_"<< i;
            addressLabel->setObjectName(QString::fromStdString(ss.str()));
            addressLabel->setText(QApplication::translate("MultiSigAddressDialog", "Address:", 0));
            address->addWidget(addressLabel);
            ss.str(std::string());

            /*create address line edit*/
            QValidatedLineEdit *addressIn = new QValidatedLineEdit(ui->addressList);
            ss << "addressIn_"<< i;
            addressIn->setObjectName(QString::fromStdString(ss.str()));
            lineEdits[i] = addressIn;
            address->addWidget(addressIn);
            ss.str(std::string());

            /*create address book button*/
            QPushButton *addressBookButton= new QPushButton(ui->addressList);
            ss << "addressBookButton_" << i;
            addressBookButton->setObjectName(QString::fromStdString(ss.str()));
            QIcon icon1;
            icon1.addFile(QStringLiteral(":/icons/address-book"), QSize(), QIcon::Normal, QIcon::Off);
            addressBookButton->setIcon(icon1);
            addressBookButton->setAutoDefault(false);
            addressBookButton->installEventFilter(this);
            address->addWidget(addressBookButton);
            ss.str(std::string());

            QPushButton *addressPasteButton = new QPushButton(ui->addressList);
            ss << "addressPasteButton_" << i;
            addressPasteButton->setObjectName(QString::fromStdString(ss.str()));
            QIcon icon2;
            icon2.addFile(QStringLiteral(":/icons/editpaste"), QSize(), QIcon::Normal, QIcon::Off);
            addressPasteButton->setIcon(icon2);
            addressPasteButton->setAutoDefault(false);
            address->addWidget(addressPasteButton);
            ss.str(std::string());

            ui->verticalLayout_4->addLayout(address);
        }

        ui->stackedWidget->setCurrentIndex(1);
    }

}

void MultiSigAddressDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec()) {
            setAddress(dlg.getReturnValue());
        }
    }
}

void MultiSigAddressDialog::on_pasteButton_clicked()
{
    setAddress(QApplication::clipboard()->text());
}

void MultiSigAddressDialog::on_createAddressButton_clicked(){
    if(!model)
        return;

    string keys[n];// = {"yDVyjR2U5UcxMsf5S4qN8EAYvn27MHxUPW", "y5vdZGbCp4SJcCiTgjYt9wYLegmHhN6iJu"};

    for(int i = 0; i < n; i++){
        keys[i] = lineEdits[i]->text().toStdString();
    }

    CScript redeem;
    try{
        createRedeemScript(m, keys);
        CScriptID innerID(redeem);
        pwalletMain->AddCScript(redeem);
        pwalletMain->SetAddressBook(innerID, "Multisig-", "receive");
        ui->exitStatus->setText(QString::fromStdString(CBitcoinAddress(innerID).ToString()));
    }catch(const runtime_error& e) {
        ui->exitStatus->setText(QString::fromStdString(e.what()));
    }
}

bool MultiSigAddressDialog::eventFilter(QObject* object, QEvent* event)
{
    /*std::size_t addrButton = (object->objectName()).toStdString().find("addressBook");
    if(addrButton != std::string::npos){
        if (model && model->getAddressTableModel()) {
            AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
            dlg.setModel(model->getAddressTableModel());
            if (dlg.exec()) {
                setAddress(dlg.getReturnValue());
            }
        }
    }*/
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        if (ui->stackedWidget->currentIndex() == 0) {
            /* Clear status message on focus change */
            ui->continueStatus->clear();
        } else if (ui->stackedWidget->currentIndex() == 1) {
            /* Clear status message on focus change */
            ui->exitStatus->clear();
        }
    }
    return QDialog::eventFilter(object, event);
}


CScript MultiSigAddressDialog::createRedeemScript(int m, string keys[])
{
    //gather pub keys
    if (n < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if (n < m)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)",
                m, n));
    if (n > 16)
        throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");

    std::vector<CPubKey> pubkeys;
    pubkeys.resize(n);

    for(int i = 0; i < n ; i++) {
        string keyString = keys[i];
#ifdef ENABLE_WALLET
        // Case 1: PIVX address and we have full public key:
        CBitcoinAddress address(keyString);
        if (pwalletMain && address.IsValid()) {
            CKeyID keyID;
            if (!address.GetKeyID(keyID)) {
                throw runtime_error(
                    strprintf("%s does not refer to a key", keyString));
            }
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s", keyString));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: " + keyString);
            pubkeys[i] = vchPubKey;
        }

        //case 2: hex pub key
        else
#endif
            if (IsHex(keyString)) {
            CPubKey vchPubKey(ParseHex(keyString));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: " + keyString);
            pubkeys[i] = vchPubKey;
        } else {
            throw runtime_error(" Invalid public key: " + keyString);
        }
    }

    return GetScriptForMultisig(m, pubkeys);
}

