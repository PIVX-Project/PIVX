#ifndef ADDRESSESWIDGET_H
#define ADDRESSESWIDGET_H

#include "addresstablemodel.h"
#include "tooltipmenu.h"
#include "furabstractlistitemdelegate.h"

#include <QWidget>

class AddressViewDelegate;
class TooltipMenu;
class PIVXGUI;
class WalletModel;

namespace Ui {
class AddressesWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class AddressesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AddressesWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~AddressesWidget();

    void setWalletModel(WalletModel *model);

    void onNewContactClicked();

private slots:
    void handleAddressClicked(const QModelIndex &index);
    void onStoreContactClicked();

    void changeTheme(bool isLightTheme, QString &theme);
private:
    Ui::AddressesWidget *ui;

    PIVXGUI* window;

    FurAbstractListItemDelegate* delegate;
    AddressTableModel* addressTablemodel;

    bool isOnMyAddresses = true;
    TooltipMenu* menu = nullptr;
};

#endif // ADDRESSESWIDGET_H
