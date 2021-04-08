//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RECEIVEWIDGET_H
#define RECEIVEWIDGET_H

#include "qt/Sombe/pwidget.h"
#include "addresstablemodel.h"
#include "qt/Sombe/furabstractlistitemdelegate.h"
#include "qt/Sombe/addressfilterproxymodel.h"

#include <QSpacerItem>
#include <QWidget>
#include <QPixmap>

class SombeGUI;
class SendCoinsRecipient;

namespace Ui {
class ReceiveWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class ReceiveWidget : public PWidget
{
    Q_OBJECT

public:
    explicit ReceiveWidget(SombeGUI* parent);
    ~ReceiveWidget();

    void loadWalletModel() override;

public Q_SLOTS:
    void onRequestClicked();
    void onMyAddressesClicked();
    void onNewAddressClicked();

private Q_SLOTS:
    void changeTheme(bool isLightTheme, QString &theme) override ;
    void onLabelClicked();
    void onCopyClicked();
    void refreshView(const QModelIndex& tl, const QModelIndex& br);
    void refreshView(QString refreshAddress = QString());
    void handleAddressClicked(const QModelIndex &index);
    void onSortChanged(int idx);
    void onSortOrderChanged(int idx);
private:
    Ui::ReceiveWidget *ui;

    FurAbstractListItemDelegate *delegate;
    AddressTableModel* addressTableModel = nullptr;
    AddressFilterProxyModel *filter = nullptr;

    QSpacerItem *spacer = nullptr;

    // Cached last address
    SendCoinsRecipient *info = nullptr;
    // Cached qr
    QPixmap *qrImage = nullptr;

    // Cached sort type and order
    AddressTableModel::ColumnIndex sortType = AddressTableModel::Label;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;

    void updateQr(QString address);
    void updateLabel();
    void showAddressGenerationDialog(bool isPaymentRequest);
    void sortAddresses();

    bool isShowingDialog = false;

};

#endif // RECEIVEWIDGET_H
