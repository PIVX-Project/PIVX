#ifndef MASTERNODESWIDGET_H
#define MASTERNODESWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/furabstractlistitemdelegate.h"
#include "qt/pivx/mnmodel.h"
#include "qt/pivx/tooltipmenu.h"

class PIVXGUI;

namespace Ui {
class MasterNodesWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class MasterNodesWidget : public PWidget
{
    Q_OBJECT

public:

    explicit MasterNodesWidget(PIVXGUI *parent = nullptr);
    ~MasterNodesWidget();

    void loadWalletModel();
private slots:
    void onCreateMNClicked();
    void changeTheme(bool isLightTheme, QString &theme);
    void onMNClicked(const QModelIndex &index);
    void onEditMNClicked();
    void onDeleteMNClicked();
    void onInfoMNClicked();

private:
    Ui::MasterNodesWidget *ui;
    FurAbstractListItemDelegate *delegate;
    MNModel *mnModel = nullptr;
    TooltipMenu* menu = nullptr;
    QModelIndex index;


    void startAlias(QString strAlias);
    void updateListState();
};

#endif // MASTERNODESWIDGET_H
