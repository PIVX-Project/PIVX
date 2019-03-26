#ifndef FURABSTRACTLISTITEMDELEGATE_H
#define FURABSTRACTLISTITEMDELEGATE_H

#include "furlistrow.h"

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
    FurAbstractListItemDelegate(int _rowHeight, FurListRow* _row, QObject *parent=nullptr);
    ~FurAbstractListItemDelegate(){};

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const override;

    FurListRow *getRowFactory();

private:
    int rowHeight = 0;
    FurListRow* row = nullptr;

};

#endif // FURABSTRACTLISTITEMDELEGATE_H
