#ifndef TXVIEWHOLDER_H
#define TXVIEWHOLDER_H

#include "qt/pivx/furlistrow.h"


class TxViewHolder : public FurListRow
{
public:
    TxViewHolder();

    explicit TxViewHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    QWidget* createHolder(int pos) override;

    QColor rectColor(bool isHovered, bool isSelected) override;

    ~TxViewHolder() override{};

    bool isLightTheme;
};

#endif // TXVIEWHOLDER_H
