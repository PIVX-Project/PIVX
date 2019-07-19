// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FURABSTRACTLISTITEMDELEGATE_H
#define FURABSTRACTLISTITEMDELEGATE_H

#include "qt/pivx/furlistrow.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QObject>
#include <QWidget>
#include <QColor>
#include <QModelIndex>
#include <QObject>
#include <QPaintEngine>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class FurAbstractListItemDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    FurAbstractListItemDelegate(int _rowHeight, FurListRow<>* _row, QObject *parent=nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const;

    FurListRow<> *getRowFactory();
private:
    int rowHeight = 0;
    FurListRow<>* row = nullptr;

};

#endif // FURABSTRACTLISTITEMDELEGATE_H
