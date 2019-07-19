// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDMULTIROW_H
#define SENDMULTIROW_H

#include <QWidget>
#include <QPushButton>
#include <QAction>
#include "walletmodel.h"
#include "amount.h"
#include "qt/pivx/pwidget.h"

class WalletModel;
class SendCoinsRecipient;

namespace Ui {
class SendMultiRow;
class QPushButton;
}

class SendMultiRow : public PWidget
{
    Q_OBJECT

public:
    explicit SendMultiRow(PWidget *parent = nullptr);
    ~SendMultiRow();

    void hideLabels();
    void showLabels();
    void setNumber(int number);
    int getNumber();

    void loadWalletModel() override;
    bool validate();
    SendCoinsRecipient getValue();
    QString getAddress();
    CAmount getAmountValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();
    CAmount getAmountValue(QString str);

    void setAddress(const QString& address);
    void setLabel(const QString& label);
    void setAmount(const QString& amount);
    void setAddressAndLabelOrDescription(const QString& address, const QString& message);
    void setFocus();

    QRect getEditLineRect();
    int getEditHeight();
    int getEditWidth();
    int getMenuBtnWidth();

public slots:
    void clear();
    void updateDisplayUnit();

signals:
    void removeEntry(SendMultiRow* entry);
    void onContactsClicked(SendMultiRow* entry);
    void onMenuClicked(SendMultiRow* entry);
    void onValueChanged();
    void onUriParsed(SendCoinsRecipient rcp);

protected:
    void resizeEvent(QResizeEvent *event) override;
    virtual void enterEvent(QEvent *) override ;
    virtual void leaveEvent(QEvent *) override ;

private slots:
    void amountChanged(const QString&);
    bool addressChanged(const QString&);
    void deleteClicked();
    //void on_payTo_textChanged(const QString& address);
    //void on_addressBookButton_clicked();

private:
    Ui::SendMultiRow *ui;
    QPushButton *iconNumber;
    QAction *btnContact;

    int displayUnit;
    int number = 0;
    bool isExpanded = false;

    SendCoinsRecipient recipient;

};

#endif // SENDMULTIROW_H
