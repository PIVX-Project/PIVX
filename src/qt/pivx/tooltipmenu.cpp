#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/forms/ui_tooltipmenu.h"
#include "qt/pivx/addnewcontactdialog.h"

#include "ui_interface.h"
#include "guiutil.h"
#include "qt/pivx/PIVXGUI.h"
#include "qt/pivx/qtutils.h"
#include <QModelIndex>
#include "addresstablemodel.h"

#include <QTimer>
#include <iostream>

TooltipMenu::TooltipMenu(PIVXGUI *_window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TooltipMenu),
    window(_window)
{
    ui->setupUi(this);

    ui->container->setProperty("cssClass", "container-list-menu");
    ui->btnCopy->setProperty("cssClass", "btn-list-menu");
    ui->btnDelete->setProperty("cssClass", "btn-list-menu");
    ui->btnEdit->setProperty("cssClass", "btn-list-menu");

    connect(ui->btnCopy, SIGNAL(clicked()), this, SLOT(copyClicked()));
    connect(ui->btnDelete, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->btnEdit, SIGNAL(clicked()), this, SLOT(editClicked()));
}

void TooltipMenu::setWalletModel(WalletModel *model){
    this->walletModel = model;
}

void TooltipMenu::setIndex(const QModelIndex &index){
    this->index = index;
}

void TooltipMenu::deleteClicked(){
    if(walletModel) {
        bool ret = false;
        // TODO: remove me..
        //emit message(tr("Delete Contact Label"), tr("Are you sure?"), CClientUIInterface::MSG_INFORMATION & CClientUIInterface::BTN_MASK, &ret);
        QString address = index.data(Qt::DisplayRole).toString();
        window->message(
                tr("Delete Contact"),
                tr("You are just about to remove the contact:\n\n%1\n\nAre you sure?").arg(address.toUtf8().constData()),
                CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL,
                &ret
        );
        if (ret) {
            if (this->walletModel->getAddressTableModel()->removeRows(index.row(), 1, index)) {
                emit message("", tr("Contact Deleted"), CClientUIInterface::MSG_INFORMATION_SNACK);
            } else {
                emit message("", tr("Error deleting a contact"), CClientUIInterface::MSG_INFORMATION_SNACK);
            }
            hide();
        }
    }
}

void TooltipMenu::copyClicked(){
    GUIUtil::setClipboard(index.data(Qt::DisplayRole).toString());
    hide();
    // TODO: change me..
    window->messageInfo(tr("Address copied"));
    //emit message("", tr("Address copied"), CClientUIInterface::MSG_INFORMATION);
}

void TooltipMenu::editClicked(){
    //TODO: Complete me..
    hide();
    QString address = index.data(Qt::DisplayRole).toString();
    QString currentLabel = index.sibling(index.row(), AddressTableModel::Label).data(Qt::DisplayRole).toString();
    window->showHide(true);
    AddNewContactDialog *dialog = new AddNewContactDialog(window);
    dialog->setData(address, currentLabel);
    if(openDialogWithOpaqueBackground(dialog, window)){
        CTxDestination dest = CBitcoinAddress(address.toStdString()).Get();
        QString label = dialog->getLabel();
        // TODO: Change this "send" type to the address type..
        if(walletModel->updateAddressBookLabels(dest, label.toStdString(), "send")){
            window->messageInfo(tr("Contact edited"));
        }else{
            window->messageInfo(tr("Contact edit failed"));
        }

    }
}

void TooltipMenu::showEvent(QShowEvent *event){
    QTimer::singleShot(5000, this, SLOT(hide()));
}

TooltipMenu::~TooltipMenu()
{
    delete ui;
}
