#ifndef MASTERNODESWIDGET_H
#define MASTERNODESWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/furabstractlistitemdelegate.h"

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
private:
    Ui::MasterNodesWidget *ui;
    FurAbstractListItemDelegate *delegate;

};

#endif // MASTERNODESWIDGET_H
