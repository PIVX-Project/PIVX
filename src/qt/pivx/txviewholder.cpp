#include "qt/pivx/txviewholder.h"
#include "qt/pivx/txrow.h"

#include "transactiontablemodel.h"
#include "bitcoingui.h"
#include <QModelIndex>

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

void TxViewHolder::init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const{
    TxRow *txRow = static_cast<TxRow*>(holder);
    txRow->updateStatus(isLightTheme, isHovered, isSelected);

    QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    bool isConfirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();

    QModelIndex indexType = index.siblingAtColumn(TransactionTableModel::Type);
    QString label = indexType.data(Qt::DisplayRole).toString() + " " + address;
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);

    txRow->setDate(date);
    txRow->setLabel(label);
    txRow->setAmount(amountText);
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
