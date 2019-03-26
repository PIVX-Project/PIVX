#ifndef TXROW_H
#define TXROW_H

#include <QWidget>

namespace Ui {
class TxRow;
}

class TxRow : public QWidget
{
    Q_OBJECT

public:
    explicit TxRow(bool isLightTheme, QWidget *parent = nullptr);
    ~TxRow();

    void sendRow(bool isLightTheme);
    void receiveRow(bool isLightTheme);
    void stakeRow(bool isLightTheme);
    void zPIVStakeRow(bool isLightTheme);
    void mintRow(bool isLightTheme);

private:
    Ui::TxRow *ui;
};

#endif // TXROW_H
