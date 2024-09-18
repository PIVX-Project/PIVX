// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_MYADDRESSROW_H
#define PIVX_QT_PIVX_MYADDRESSROW_H

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

    void updateView(const QString& address, const QString& label, const QString& date);

private:
    Ui::MyAddressRow *ui;
};

#endif // PIVX_QT_PIVX_MYADDRESSROW_H
