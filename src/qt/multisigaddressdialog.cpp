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
#include "rpcconsole.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

#include "init.h"
#include "wallet.h"

#include <string>
#include <vector>
#include <boost/format.hpp>

#include <QtCore/QVariant>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QClipboard>
#include "qvalidatedlineedit.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

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

        for(int i = 1; i < n; i++){
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

extern CScript _createmultisig_redeemScript(const Array& params);

void MultiSigAddressDialog::on_createAddressButton_clicked(){
    if(!model)
        return;

    // model->validateAddress(ui->addressIn_1->text());
    CBasicKeyStore keystore;
    CKey key[4];
    std::vector<CPubKey> keys;
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
        keys.push_back(key[i].GetPubKey());
    }

    Object params;
    params.push_back(Value(2));
    params.push_back(Value("[\"yDVyjR2U5UcxMsf5S4qN8EAYvn27MHxUPW\", \"y5vdZGbCp4SJcCiTgjYt9wYLegmHhN6iJu\"]"));

    std::stringstream ss;
    write(params, ss, pretty_print);

    // const Array& params = {1,"[\"xw6hdzY1zGvD17PiVVgFbHTCzw8kuqveHq\",\"xw7mAPxpGd6pG6uBpcXT4KVrNzcfprTTDF\"]"};
    CScript redeem = _createmultisig_redeemScript(ss.str());
    CScriptID innerID(redeem);
    pwalletMain->AddCScript(redeem);


    pwalletMain->SetAddressBook(innerID, "testinglollolyouasrelol", "testingFROMCODE");


    ui->exitStatus->setText(QString::fromStdString(CBitcoinAddress(innerID).ToString()));

}

bool MultiSigAddressDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        if (ui->stackedWidget->currentIndex() == 0) {
            /* Clear status message on focus change */
            ui->continueStatus->clear();

            /* Select generated signature
            if (object == ui->encryptedKeyOut_ENC) {
                ui->encryptedKeyOut_ENC->selectAll();
                return true;
            } */
        } else if (ui->stackedWidget->currentIndex() == 1) {
            /* Clear status message on focus change */
            ui->exitStatus->clear();
        }
    }
    return QDialog::eventFilter(object, event);
}

