#ifndef FURLISTROW_H
#define FURLISTROW_H

#include <QWidget>
#include <QColor>

class FurListRow{

public:
    FurListRow(){}
    virtual ~FurListRow(){}

    virtual QWidget* createHolder(int){
       return nullptr;
    }

    virtual QColor rectColor(bool isHovered, bool isSelected){
        return QColor();
    }
};

#endif // FURLISTROW_H
