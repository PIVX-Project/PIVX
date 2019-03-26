#include "qt/pivx/txviewholder.h"
#include "qt/pivx/txrow.h"

QWidget* TxViewHolder::createHolder(int pos){
    TxRow *row = new TxRow(isLightTheme);
    // TODO: move this to other class
    if(pos % 5 == 2){
        row->sendRow(isLightTheme);
    }else if(pos % 5 == 1){
        row->stakeRow(isLightTheme);
    }else if (pos % 5 == 0){
        row->receiveRow(isLightTheme);
    }else if (pos % 5 == 3){
        row->zPIVStakeRow(isLightTheme);
    }else{
        row->mintRow(isLightTheme);
    }
    return row;
}
QColor TxViewHolder::rectColor(bool isHovered, bool isSelected){
    // TODO: Move this to other class
    if(isLightTheme){
        if (isSelected) {
            return QColor("#25b088ff");
        }else if(isHovered){
            return QColor("#25bababa");
        } else{
            return QColor("#ffffff");
        }
    }else{
        if (isSelected) {
            return QColor("#25b088ff");
        }else if(isHovered){
            return QColor("#25bababa");
        } else{
            return QColor("#0f0b16");
        }
    }
}
