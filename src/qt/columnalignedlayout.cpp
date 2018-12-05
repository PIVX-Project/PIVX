// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The PHORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "columnalignedlayout.h"
#include <QHeaderView>

ColumnAlignedLayout::ColumnAlignedLayout() : QHBoxLayout() {}

ColumnAlignedLayout::ColumnAlignedLayout(QWidget *parent) : QHBoxLayout(parent) {}

void ColumnAlignedLayout::setGeometry(const QRect &r)
{
    QHBoxLayout::setGeometry(r);

    if (!headerView) {
        return;
    }

    int widgetX = parentWidget()->mapToGlobal(QPoint(0, 0)).x();
    int headerX = headerView->mapToGlobal(QPoint(0, 0)).x();
    int delta = headerX - widgetX;

    for (int ii = 0; ii < headerView->count(); ++ii) {
        int pos = headerView->sectionViewportPosition(ii);
        int size = headerView->sectionSize(ii);

        auto item = itemAt(ii);
        auto r = item->geometry();
        r.setLeft(pos + delta);
        r.setWidth(size);
        item->setGeometry(r);
    }
}
