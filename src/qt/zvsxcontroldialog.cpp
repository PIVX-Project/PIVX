// Copyright (c) 2017 The Vsync developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zvsxcontroldialog.h"
#include "ui_zvsxcontroldialog.h"

#include "main.h"
#include "walletmodel.h"

using namespace std;

std::list<std::string> ZVSXControlDialog::listSelectedMints;
std::list<CZerocoinMint> ZVSXControlDialog::listMints;

ZVSXControlDialog::ZVSXControlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZVSXControlDialog),
    model(0)
{
    ui->setupUi(this);
    listMints.clear();
    privacyDialog = (PrivacyDialog*)parent;

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(updateSelection(QTreeWidgetItem*, int)));

    // push select/deselect all button
    connect(ui->pushButtonAll, SIGNAL(clicked()), this, SLOT(ButtonAllClicked()));
}

ZVSXControlDialog::~ZVSXControlDialog()
{
    delete ui;
}

void ZVSXControlDialog::setModel(WalletModel *model)
{
    this->model = model;
    updateList();
}

//Update the tree widget
void ZVSXControlDialog::updateList()
{
    // need to prevent the slot from being called each time something is changed
    ui->treeWidget->blockSignals(true);
    ui->treeWidget->clear();

    // add a top level item for each denomination
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
    map<libzerocoin::CoinDenomination, int> mapDenomPosition;
    for (auto denom : libzerocoin::zerocoinDenomList) {
        QTreeWidgetItem* itemDenom(new QTreeWidgetItem);
        ui->treeWidget->addTopLevelItem(itemDenom);

        //keep track of where this is positioned in tree widget
        mapDenomPosition[denom] = ui->treeWidget->indexOfTopLevelItem(itemDenom);

        itemDenom->setFlags(flgTristate);
        itemDenom->setText(COLUMN_DENOMINATION, QString::number(denom));
    }

    // select all unused coins - including not mature. Update status of coins too.
    std::list<CZerocoinMint> list;
    model->listZerocoinMints(list, true, false, true);
    this->listMints = list;

    //populate rows with mint info
    for(const CZerocoinMint mint : listMints) {
        // assign this mint to the correct denomination in the tree view
        libzerocoin::CoinDenomination denom = mint.GetDenomination();
        QTreeWidgetItem *itemMint = new QTreeWidgetItem(ui->treeWidget->topLevelItem(mapDenomPosition.at(denom)));

        // if the mint is already selected, then it needs to have the checkbox checked
        std::string strPubCoin = mint.GetValue().GetHex();
        itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        if(count(listSelectedMints.begin(), listSelectedMints.end(), strPubCoin))
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Checked);

        itemMint->setText(COLUMN_DENOMINATION, QString::number(mint.GetDenomination()));
        itemMint->setText(COLUMN_PUBCOIN, QString::fromStdString(strPubCoin));

        int nConfirmations = (mint.GetHeight() ? chainActive.Height() - mint.GetHeight() : 0);
        if (nConfirmations < 0) {
            // Sanity check
            nConfirmations = 0;
        }

        itemMint->setText(COLUMN_CONFIRMATIONS, QString::number(nConfirmations));

        // check to make sure there are at least 3 other mints added to the accumulators after this
        int nMintsAdded = 0;
        if(mint.GetHeight() != 0 && mint.GetHeight() < chainActive.Height() - 2) {
            CBlockIndex *pindex = chainActive[mint.GetHeight() + 1];
            while(pindex->nHeight < chainActive.Height() - 30) { // 30 just to make sure that its at least 2 checkpoints from the top block
                nMintsAdded += count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), mint.GetDenomination());
                if(nMintsAdded >= Params().Zerocoin_RequiredAccumulation())
                    break;
                pindex = chainActive[pindex->nHeight + 1];
            }
        }

        // disable selecting this mint if it is not spendable - also display a reason why
        bool fSpendable = nMintsAdded >= Params().Zerocoin_RequiredAccumulation() && nConfirmations >= Params().Zerocoin_MintRequiredConfirmations();
        if(!fSpendable) {
            itemMint->setDisabled(true);
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            //if this mint is in the selection list, then remove it
            auto it = std::find(listSelectedMints.begin(), listSelectedMints.end(), mint.GetValue().GetHex());
            if (it != listSelectedMints.end()) {
                listSelectedMints.erase(it);
            }

            string strReason = "";
            if(nConfirmations < Params().Zerocoin_MintRequiredConfirmations())
                strReason = strprintf("Needs %d more confirmations", Params().Zerocoin_MintRequiredConfirmations() - nConfirmations);
            else
                strReason = strprintf("Needs %d more mints added to network", Params().Zerocoin_RequiredAccumulation() - nMintsAdded);

            itemMint->setText(COLUMN_ISSPENDABLE, QString::fromStdString(strReason));
        } else {
            itemMint->setText(COLUMN_ISSPENDABLE, QString("Yes"));
        }
    }

    ui->treeWidget->blockSignals(false);
    updateLabels();
}

// Update the list when a checkbox is clicked
void ZVSXControlDialog::updateSelection(QTreeWidgetItem* item, int column)
{
    // only want updates from non top level items that are available to spend
    if (item->parent() && column == COLUMN_CHECKBOX && !item->isDisabled()){

        // see if this mint is already selected in the selection list
        std::string strPubcoin = item->text(COLUMN_PUBCOIN).toStdString();
        auto iter = std::find(listSelectedMints.begin(), listSelectedMints.end(), strPubcoin);
        bool fSelected = iter != listSelectedMints.end();

        // set the checkbox to the proper state and add or remove the mint from the selection list
        if (item->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
            if (fSelected) return;
            listSelectedMints.emplace_back(strPubcoin);
        } else {
            if (!fSelected) return;
            listSelectedMints.erase(iter);
        }
        updateLabels();
    }
}

// Update the Quantity and Amount display
void ZVSXControlDialog::updateLabels()
{
    int64_t nAmount = 0;
    for (const CZerocoinMint mint : listMints) {
        if (count(listSelectedMints.begin(), listSelectedMints.end(), mint.GetValue().GetHex())) {
            nAmount += mint.GetDenomination();
        }
    }

    //update this dialog's labels
    ui->labelZVSX_int->setText(QString::number(nAmount));
    ui->labelQuantity_int->setText(QString::number(listSelectedMints.size()));

    //update PrivacyDialog labels
    privacyDialog->setZVSXControlLabels(nAmount, listSelectedMints.size());
}

std::vector<CZerocoinMint> ZVSXControlDialog::GetSelectedMints()
{
    std::vector<CZerocoinMint> listReturn;
    for (const CZerocoinMint mint : listMints) {
        if (count(listSelectedMints.begin(), listSelectedMints.end(), mint.GetValue().GetHex())) {
            listReturn.emplace_back(mint);
        }
    }

    return listReturn;
}

// select or deselect all of the mints
void ZVSXControlDialog::ButtonAllClicked()
{
    ui->treeWidget->blockSignals(true);
    Qt::CheckState state = Qt::Checked;
    for(int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if(ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }

    //much quicker to start from scratch than to have QT go through all the objects and update
    ui->treeWidget->clear();

    if(state == Qt::Checked) {
        for(const CZerocoinMint mint : listMints)
            listSelectedMints.emplace_back(mint.GetValue().GetHex());
    } else {
        listSelectedMints.clear();
    }

    updateList();
}
