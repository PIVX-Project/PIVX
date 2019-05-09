#ifndef TXROW_H
#define TXROW_H

#include <QWidget>
#include <QDateTime>
#include "transactionrecord.h"

namespace Ui {
class TxRow;
}

class TxRow : public QWidget
{
    Q_OBJECT

public:
    explicit TxRow(bool isLightTheme, QWidget *parent = nullptr);
    ~TxRow();

    void updateStatus(bool isLightTheme, bool isHover, bool isSelected);

    void sendRow(bool isLightTheme);
    void receiveRow(bool isLightTheme);
    void stakeRow(bool isLightTheme);
    void zPIVStakeRow(bool isLightTheme);
    void mintRow(bool isLightTheme);

    void setDate(QDateTime);
    void setLabel(QString);
    void setAmount(QString);
    void setType(bool isLightTheme, int type, bool isConfirmed);
    void setConfirmStatus(bool isConfirmed);

private:
    Ui::TxRow *ui;
    bool isConfirmed = false;
};

#endif // TXROW_H
