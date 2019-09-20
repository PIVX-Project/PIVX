// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/contactsdropdown.h"

#include <QPainter>
#include <QSizePolicy>
#include "qt/pivx/addresslabelrow.h"
#include "qt/pivx/contactdropdownrow.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/furlistrow.h"
#include "walletmodel.h"
#include "addresstablemodel.h"

#define DECORATION_SIZE 70
#define NUM_ITEMS 3

class ContViewHolder : public FurListRow<QWidget*>
{
public:
    ContViewHolder();

    explicit ContViewHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    ContactDropdownRow* createHolder(int pos) override{
        if (!row) row = new ContactDropdownRow();
        row->init(true, false);
        return row;
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override{
        ContactDropdownRow* row = static_cast<ContactDropdownRow*>(holder);
        row->update(isLightTheme, isHovered, isSelected);
        QString address = index.data(Qt::DisplayRole).toString();
        QModelIndex sibling = index.sibling(index.row(), AddressTableModel::Label);
        QString label = sibling.data(Qt::DisplayRole).toString();
        row->setData(address, label);
    }

    QColor rectColor(bool isHovered, bool isSelected) override{
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~ContViewHolder() override{}

    bool isLightTheme;
    ContactDropdownRow* row = nullptr;
};

ContactsDropdown::ContactsDropdown(int minWidth, int minHeight, PWidget *parent) :
   PWidget(parent)
{

    this->setStyleSheet(parent->styleSheet());

    delegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                new ContViewHolder(isLightTheme()),
                this
    );

    setMinimumWidth(minWidth);
    setMinimumHeight(minHeight);
    setContentsMargins(0,0,0,0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    frameList = new QFrame(this);
    frameList->setProperty("cssClass", "container-border-light");
    frameList->setContentsMargins(10,10,10,10);
    frameList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    list = new QListView(frameList);
    list->setMinimumWidth(minWidth);
    list->setProperty("cssClass", "container-border-light");
    list->setItemDelegate(delegate);
    list->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    list->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    list->setAttribute(Qt::WA_MacShowFocusRect, false);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(list, SIGNAL(clicked(QModelIndex)), this, SLOT(handleClick(QModelIndex)));
}

void ContactsDropdown::setWalletModel(WalletModel* _model, QString type){
    model = _model->getAddressTableModel();
    this->filter = new AddressFilterProxyModel(type, this);
    this->filter->setSourceModel(model);
    list->setModel(this->filter);
    list->setModelColumn(AddressTableModel::Address);
}

void ContactsDropdown::resizeList(int minWidth, int mintHeight){
    list->setMinimumWidth(minWidth);
    setMinimumWidth(minWidth);
    setMinimumHeight(mintHeight);
    frameList->setMinimumHeight(mintHeight);
    frameList->setMinimumWidth(minWidth);
    list->setMinimumHeight(mintHeight);
    list->resize(mintHeight,mintHeight);
    list->adjustSize();
    frameList->resize(minWidth, mintHeight);
    resize(minWidth, mintHeight);
    adjustSize();
    update();
}

void ContactsDropdown::handleClick(const QModelIndex &index){
    QModelIndex rIndex = (filter) ? filter->mapToSource(index) : index;
    QString address = rIndex.data(Qt::DisplayRole).toString();
    QModelIndex sibling = rIndex.sibling(rIndex.row(), AddressTableModel::Label);
    QString label = sibling.data(Qt::DisplayRole).toString();
    emit contactSelected(address, label);
    close();
}

void ContactsDropdown::changeTheme(bool isLightTheme, QString& theme){
    static_cast<ContViewHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
}
