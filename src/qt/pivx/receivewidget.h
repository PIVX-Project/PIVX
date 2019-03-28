#ifndef RECEIVEWIDGET_H
#define RECEIVEWIDGET_H

#include "addresstablemodel.h"
#include "qt/pivx/furabstractlistitemdelegate.h"

#include <QSpacerItem>
#include <QWidget>

class PIVXGUI;
class WalletModel;

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
private:
    Ui::ReceiveWidget *ui;
    PIVXGUI* window;

    FurAbstractListItemDelegate *delegate;
    AddressTableModel* addressTableModel;

    QSpacerItem *spacer = nullptr;

};

#endif // RECEIVEWIDGET_H
