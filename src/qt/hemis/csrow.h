// Copyright (c) 2019 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CSROW_H
#define CSROW_H

#include <QWidget>

namespace Ui {
class CSRow;
}

class CSRow : public QWidget
{
    Q_OBJECT

public:
    explicit CSRow(QWidget *parent = nullptr);
    ~CSRow();

    void updateView(const QString& address, const QString& label, bool isStaking, bool isReceivedDelegation, const QString& amount);
    void updateState(bool isLightTheme, bool isHovered, bool isSelected);
    void showMenuButton(bool show);
protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

private:
    Ui::CSRow *ui;

    bool fShowMenuButton = true;
};

#endif // CSROW_H
