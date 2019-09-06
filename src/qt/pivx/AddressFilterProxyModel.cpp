// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/addressfilterproxymodel.h"

bool AddressFilterProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    auto model = sourceModel();
    auto label = model->index(row, AddressTableModel::Label, parent);

    if (model->data(label, AddressTableModel::TypeRole).toString() != m_type) {
        return false;
    }

    auto address = model->index(row, AddressTableModel::Address, parent);

    if (filterRegExp().indexIn(model->data(address).toString()) < 0 &&
        filterRegExp().indexIn(model->data(label).toString()) < 0) {
        return false;
    }

    return true;
}

int AddressFilterProxyModel::rowCount(const QModelIndex& parent) const
{
    return QSortFilterProxyModel::rowCount(parent);
}
