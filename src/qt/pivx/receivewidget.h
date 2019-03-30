#ifndef RECEIVEWIDGET_H
#define RECEIVEWIDGET_H

#include "addresstablemodel.h"
#include "qt/pivx/furabstractlistitemdelegate.h"

#include <QSpacerItem>
#include <QWidget>
#include <QPixmap>

class PIVXGUI;
class WalletModel;
class SendCoinsRecipient;

namespace Ui {
class ReceiveWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class ReceiveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReceiveWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~ReceiveWidget();

    void setWalletModel(WalletModel* model);

public slots:
    void onRequestClicked();
    void onMyAddressesClicked();
    
private slots:
    void changeTheme(bool isLightTheme, QString &theme);
    void onLabelClicked();
    void onCopyClicked();
private:
    Ui::ReceiveWidget *ui;
    PIVXGUI* window;

    FurAbstractListItemDelegate *delegate;
    WalletModel *walletModel = nullptr;
    AddressTableModel* addressTableModel = nullptr;

    QSpacerItem *spacer = nullptr;

    // Cached last address, todo: use a pointer here..
    SendCoinsRecipient *info = nullptr;
    // Cached qr
    QPixmap *qrImage = nullptr;

    void updateQr(QString address);

};

#endif // RECEIVEWIDGET_H
