// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_FURLISTROW_H
#define PIVX_QT_PIVX_FURLISTROW_H

#include <QWidget>
#include <QColor>

template <class T=QWidget*>
class FurListRow{

public:
    FurListRow(){}
    virtual inline ~FurListRow(){}

    virtual inline T createHolder(int){
       return nullptr;
    }

    virtual inline void init(T, const QModelIndex&, bool isHovered, bool isSelected) const{

    }

    virtual inline QColor rectColor(bool isHovered, bool isSelected){
        return QColor();
    }
};

#endif // PIVX_QT_PIVX_FURLISTROW_H
