//
// Created by furszy on 4/3/19.
//

#include "qt/pivx/AddressFilterProxyModel.h"


bool AddressFilterProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const {
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
