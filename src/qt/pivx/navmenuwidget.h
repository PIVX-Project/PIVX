#ifndef NAVMENUWIDGET_H
#define NAVMENUWIDGET_H

#include <QWidget>

class PIVXGUI;

namespace Ui {
class NavMenuWidget;
}

class NavMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavMenuWidget(PIVXGUI* mainWindow, QWidget *parent = nullptr);
    ~NavMenuWidget();

private slots:
    void onSendClicked();
    void onDashboardClicked();
    void onPrivacyClicked();
    void onAddressClicked();
    void onMasterNodesClicked();
    void onSettingsClicked();
    void onReceiveClicked();
private:
    Ui::NavMenuWidget *ui;

    PIVXGUI* window;
};

#endif // NAVMENUWIDGET_H
