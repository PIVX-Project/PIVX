// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The PHORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COLUMNALIGNEDLAYOUT_H
#define COLUMNALIGNEDLAYOUT_H

#include <QHBoxLayout>

class QHeaderView;

class ColumnAlignedLayout : public QHBoxLayout
{
    Q_OBJECT
public:
    ColumnAlignedLayout();
    explicit ColumnAlignedLayout(QWidget *parent);
    void setTableColumnsToTrack(QHeaderView *view) { headerView = view; }

private:
    void setGeometry(const QRect &r);
    QHeaderView *headerView;
};

#endif // COLUMNALIGNEDLAYOUT_H
