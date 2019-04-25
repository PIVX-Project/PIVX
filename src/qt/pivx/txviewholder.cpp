#include "qt/pivx/txviewholder.h"
#include "qt/pivx/txrow.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include <QModelIndex>
#include <iostream>

#define ADDRESS_SIZE 12

QWidget* TxViewHolder::createHolder(int pos){
    return new TxRow(isLightTheme);
}

void TxViewHolder::init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const{
    TxRow *txRow = static_cast<TxRow*>(holder);
    txRow->updateStatus(isLightTheme, isHovered, isSelected);

    QModelIndex rIndex = (filter) ? filter->mapToSource(index) : index;
    TransactionRecord *rec = static_cast<TransactionRecord*>(rIndex.internalPointer());
    QDateTime date = rIndex.data(TransactionTableModel::DateRole).toDateTime();
    QString address = rIndex.data(Qt::DisplayRole).toString();
    qint64 amount = rIndex.data(TransactionTableModel::AmountRole).toLongLong();
    bool isConfirmed = rIndex.data(TransactionTableModel::ConfirmedRole).toBool();
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);
    QModelIndex indexType = rIndex.sibling(rIndex.row(),TransactionTableModel::Type);
    if(address.length() > 20) {
        address = address.left(ADDRESS_SIZE) + "..." + address.right(ADDRESS_SIZE);
    }
    QString label = indexType.data(Qt::DisplayRole).toString() + " " + address;

    //std::cout << "label: " << label.toStdString() << ", type: " << rec->type << std::endl;

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
