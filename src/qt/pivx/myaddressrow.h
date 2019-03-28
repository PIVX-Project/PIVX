#ifndef MYADDRESSROW_H
#define MYADDRESSROW_H

#include <QWidget>

namespace Ui {
class MyAddressRow;
}

class MyAddressRow : public QWidget
{
    Q_OBJECT

public:
    explicit MyAddressRow(QWidget *parent = nullptr);
    ~MyAddressRow();

private:
    Ui::MyAddressRow *ui;
};

#endif // MYADDRESSROW_H
