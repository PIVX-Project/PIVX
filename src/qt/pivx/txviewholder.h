#ifndef TXVIEWHOLDER_H
#define TXVIEWHOLDER_H

#include "qt/pivx/furlistrow.h"
#include "bitcoinunits.h"

class TxViewHolder : public FurListRow<QWidget*>
{
public:
    TxViewHolder();

    explicit TxViewHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    QWidget* createHolder(int pos) override;

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override;

    QColor rectColor(bool isHovered, bool isSelected) override;

    ~TxViewHolder() override{};

    bool isLightTheme;

    void setDisplayUnit(int displayUnit){
        nDisplayUnit = displayUnit;
    }

private:
    int nDisplayUnit;
};

#endif // TXVIEWHOLDER_H
