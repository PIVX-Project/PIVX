#ifndef ADDRESSLABELROW_H
#define ADDRESSLABELROW_H

#include <QWidget>

namespace Ui {
class AddressLabelRow;
}

class AddressLabelRow : public QWidget
{
    Q_OBJECT

public:
    explicit AddressLabelRow(bool isLightTheme, bool isHover , QWidget *parent = nullptr);
    ~AddressLabelRow();

    void updateState(bool isLightTheme, bool isHovered, bool isSelected);
protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

private:
    Ui::AddressLabelRow *ui;
};

#endif // ADDRESSLABELROW_H
