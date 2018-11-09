// Copyright (c) 2017-2018 The XVIP developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "multisenddialog.h"
#include "ui_multisenddialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "base58.h"
#include "init.h"
#include "walletmodel.h"

#include <QStyle>

MultiSendDialog::MultiSendDialog(QWidget* parent) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                    ui(new Ui::MultiSendDialog),
                                                    model(nullptr)
{
    ui->setupUi(this);
    updateCheckBoxes();
}

MultiSendDialog::~MultiSendDialog()
{
    delete ui;
}

void MultiSendDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void MultiSendDialog::setAddress(const QString& address)
{
    setAddress(address, ui->multiSendAddressEdit);
}

void MultiSendDialog::setAddress(const QString& address, QLineEdit* addrEdit)
{
    addrEdit->setText(address);
    addrEdit->setFocus();
}

void MultiSendDialog::updateCheckBoxes()
{
    ui->multiSendStakeCheckBox->setChecked(pwalletMain->fMultiSendStake);
    ui->multiSendMasternodeCheckBox->setChecked(pwalletMain->fMultiSendMasternodeReward);
}

void MultiSendDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
            setAddress(dlg.getReturnValue(), ui->multiSendAddressEdit);

        // Update the label text box with the label in the addressbook
        QString associatedLabel = model->getAddressTableModel()->labelForAddress(dlg.getReturnValue());
        if (!associatedLabel.isEmpty())
            ui->labelAddressLabelEdit->setText(associatedLabel);
        else
            ui->labelAddressLabelEdit->setText(tr("(no label)"));
    }
}

void MultiSendDialog::on_viewButton_clicked()
{
    std::pair<std::string, int> pMultiSend;
    std::string strMultiSendPrint;
    QString strStatus;
    if (pwalletMain->isMultiSendEnabled()) {
        if (pwalletMain->fMultiSendStake && pwalletMain->fMultiSendMasternodeReward)
            strStatus += tr("MultiSend Active for Stakes and Masternode Rewards") + "\n";
        else if (pwalletMain->fMultiSendStake)
            strStatus += tr("MultiSend Active for Stakes") + "\n";
        else if (pwalletMain->fMultiSendMasternodeReward)
            strStatus += tr("MultiSend Active for Masternode Rewards") + "\n";
    } else
        strStatus += tr("MultiSend Not Active") + "\n";

    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        pMultiSend = pwalletMain->vMultiSend[i];
        if (model && model->getAddressTableModel()) {
            std::string associatedLabel;
            associatedLabel = model->getAddressTableModel()->labelForAddress(pMultiSend.first.c_str()).toStdString();
            strMultiSendPrint += associatedLabel.c_str();
            strMultiSendPrint += " - ";
        }
        strMultiSendPrint += pMultiSend.first.c_str();
        strMultiSendPrint += " - ";
        strMultiSendPrint += std::to_string(pMultiSend.second);
        strMultiSendPrint += "%\n";
    }
    ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
    ui->message->setText(strStatus + QString(strMultiSendPrint.c_str()));
}

void MultiSendDialog::on_addButton_clicked()
{
    bool fValidConversion = false;
    std::string strAddress = ui->multiSendAddressEdit->text().toStdString();
    if (!CBitcoinAddress(strAddress).IsValid()) {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The entered address: %1 is invalid.\nPlease check the address and try again.").arg(ui->multiSendAddressEdit->text()));
        ui->multiSendAddressEdit->setFocus();
        return;
    }
    int nMultiSendPercent = ui->multiSendPercentEdit->text().toInt(&fValidConversion, 10);
    int nSumMultiSend = 0;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
        nSumMultiSend += pwalletMain->vMultiSend[i].second;
    if (nSumMultiSend + nMultiSendPercent > 100) {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The total amount of your MultiSend vector is over 100% of your stake reward"));
        ui->multiSendAddressEdit->setFocus();
        return;
    }
    if (!fValidConversion || nMultiSendPercent > 100 || nMultiSendPercent <= 0) {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("Please Enter 1 - 100 for percent."));
        ui->multiSendPercentEdit->setFocus();
        return;
    }
    std::pair<std::string, int> pMultiSend;
    pMultiSend.first = strAddress;
    pMultiSend.second = nMultiSendPercent;
    pwalletMain->vMultiSend.push_back(pMultiSend);
    ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
    std::string strMultiSendPrint;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        pMultiSend = pwalletMain->vMultiSend[i];
        strMultiSendPrint += pMultiSend.first.c_str();
        strMultiSendPrint += " - ";
        strMultiSendPrint += std::to_string(pMultiSend.second);
        strMultiSendPrint += "%\n";
    }

    if (model && model->getAddressTableModel()) {
        // update the address book with the label given or no label if none was given.
        CBitcoinAddress address(strAddress);
        std::string userInputLabel = ui->labelAddressLabelEdit->text().toStdString();
        if (!userInputLabel.empty())
            model->updateAddressBookLabels(address.Get(), userInputLabel, "send");
        else
            model->updateAddressBookLabels(address.Get(), "(no label)", "send");
    }

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if(!walletdb.WriteMultiSend(pwalletMain->vMultiSend)) {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("Saved the MultiSend to memory, but failed saving properties to the database."));
        ui->multiSendAddressEdit->setFocus();
        return;
    }
    ui->message->setText(tr("MultiSend Vector") + "\n" + QString(strMultiSendPrint.c_str()));
}

void MultiSendDialog::on_deleteButton_clicked()
{
    std::vector<std::pair<std::string, int> > vMultiSendTemp = pwalletMain->vMultiSend;
    std::string strAddress = ui->multiSendAddressEdit->text().toStdString();
    bool fRemoved = false;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        if (pwalletMain->vMultiSend[i].first == strAddress) {
            pwalletMain->vMultiSend.erase(pwalletMain->vMultiSend.begin() + i);
            fRemoved = true;
        }
    }
    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.EraseMultiSend(vMultiSendTemp))
        fRemoved = false;
    if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
        fRemoved = false;

    if (fRemoved)
        ui->message->setText(tr("Removed %1").arg(QString(strAddress.c_str())));
    else
        ui->message->setText(tr("Could not locate address"));

    updateCheckBoxes();
}

void MultiSendDialog::on_activateButton_clicked()
{
    QString strRet;
    if (pwalletMain->vMultiSend.size() < 1)
        strRet = tr("Unable to activate MultiSend, check MultiSend vector");
    else if (!(ui->multiSendStakeCheckBox->isChecked() || ui->multiSendMasternodeCheckBox->isChecked())) {
        strRet = tr("Need to select to send on stake and/or masternode rewards");
    } else if (CBitcoinAddress(pwalletMain->vMultiSend[0].first).IsValid()) {
        pwalletMain->fMultiSendStake = ui->multiSendStakeCheckBox->isChecked();
        pwalletMain->fMultiSendMasternodeReward = ui->multiSendMasternodeCheckBox->isChecked();

        CWalletDB walletdb(pwalletMain->strWalletFile);
        if (!walletdb.WriteMSettings(pwalletMain->fMultiSendStake, pwalletMain->fMultiSendMasternodeReward, pwalletMain->nLastMultiSendHeight))
            strRet = tr("MultiSend activated but writing settings to DB failed");
        else
            strRet = tr("MultiSend activated");
    } else
        strRet = tr("First Address Not Valid");
    ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
    ui->message->setText(strRet);
}

void MultiSendDialog::on_disableButton_clicked()
{
    QString strRet;
    pwalletMain->setMultiSendDisabled();
    CWalletDB walletdb(pwalletMain->strWalletFile);

    if (!walletdb.WriteMSettings(false, false, pwalletMain->nLastMultiSendHeight))
        strRet = tr("MultiSend deactivated but writing settings to DB failed");
    else
        strRet = tr("MultiSend deactivated");

    ui->message->setProperty("status", "");
    ui->message->style()->polish(ui->message);
    ui->message->setText(strRet);
}
