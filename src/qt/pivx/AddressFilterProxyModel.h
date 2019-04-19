//
// Created by furszy on 4/3/19.
//

#ifndef PIVX_CORE_NEW_GUI_ADDRESSFILTERPROXYMODEL_H
#define PIVX_CORE_NEW_GUI_ADDRESSFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include "addresstablemodel.h"


class AddressFilterProxyModel final : public QSortFilterProxyModel
{
    const QString m_type;

public:
    AddressFilterProxyModel(const QString& type, QObject* parent)
            : QSortFilterProxyModel(parent)
            , m_type(type) {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortCaseSensitivity(Qt::CaseInsensitive);
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
};


#endif //PIVX_CORE_NEW_GUI_ADDRESSFILTERPROXYMODEL_H
