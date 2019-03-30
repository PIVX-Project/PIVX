#include "qt/pivx/txviewholder.h"
#include "qt/pivx/txrow.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "bitcoingui.h"
#include <QModelIndex>
#include <iostream>

QWidget* TxViewHolder::createHolder(int pos){
    return new TxRow(isLightTheme);
}

void TxViewHolder::init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const{
    TxRow *txRow = static_cast<TxRow*>(holder);
    txRow->updateStatus(isLightTheme, isHovered, isSelected);

    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());
    QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    bool isConfirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);
    QModelIndex indexType = index.siblingAtColumn(TransactionTableModel::Type);
    QString label = indexType.data(Qt::DisplayRole).toString() + " " + address;


    txRow->setDate(date);
    txRow->setLabel(label);
    txRow->setAmount(amountText);
    txRow->setType(isLightTheme, rec->type);
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
