// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/addressholder.h"
#include "qt/pivx/qtutils.h"

void AddressHolder::init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const {
    MyAddressRow *row = static_cast<MyAddressRow*>(holder);
    QString address = index.data(Qt::DisplayRole).toString();
    if (index.data(AddressTableModel::TypeRole).toString() == AddressTableModel::ShieldedReceive) {
        address = address.left(22) + "..." + address.right(22);
    }
    QString label = index.sibling(index.row(), AddressTableModel::Label).data(Qt::DisplayRole).toString();
    uint time = index.sibling(index.row(), AddressTableModel::Date).data(Qt::DisplayRole).toUInt();
    QString date = (time == 0) ? "" : GUIUtil::dateTimeStr(QDateTime::fromTime_t(time));
    row->updateView(address, label, date);
}

QColor AddressHolder::rectColor(bool isHovered, bool isSelected) {
    return getRowColor(isLightTheme, isHovered, isSelected);
}
