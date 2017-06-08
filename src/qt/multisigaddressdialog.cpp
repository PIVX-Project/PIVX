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

#include "init.h"
#include "wallet.h"

#include <string>
#include <vector>

#include <QClipboard>

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

void MultiSigAddressDialog::on_continueButton_clicked()
{
    if(!model)
       return;

    int m = ui->enterMSpinbox->value();
    int n = ui->enterNSpinbox->value();

    if(m > n) {
        ui->continueStatus->setStyleSheet("QLabel { color: red; }");
        ui->continueStatus->setText("The amount of required signatures must be less than or equal to the total number of addresses.");
    } else {
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
