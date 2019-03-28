#ifndef PRIVACYWIDGET_H
#define PRIVACYWIDGET_H

#include "qt/pivx/furabstractlistitemdelegate.h"
#include "transactiontablemodel.h"

#include <QLabel>
#include <QWidget>

class PIVXGUI;
class WalletModel;

namespace Ui {
class PrivacyWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class PrivacyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PrivacyWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~PrivacyWidget();

    void setWalletModel(WalletModel *_model);
private slots:
    void changeTheme(bool isLightTheme, QString &theme);
    void onCoinControlClicked();
    void onDenomClicked();
    void onRescanMintsClicked();
    void onResetZeroClicked();
    void onTotalZpivClicked();
private:
    Ui::PrivacyWidget *ui;
    PIVXGUI* window;
    FurAbstractListItemDelegate *delegate = nullptr;
    TransactionTableModel* model = nullptr;
};

#endif // PRIVACYWIDGET_H
