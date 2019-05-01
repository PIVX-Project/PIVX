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

public slots:
    void selectSettings();

private slots:
    void onSendClicked();
    void onDashboardClicked();
    void onPrivacyClicked();
    void onAddressClicked();
    void onMasterNodesClicked();
    void onSettingsClicked();
    void onReceiveClicked();
    void updateButtonStyles();
private:
    Ui::NavMenuWidget *ui;

    PIVXGUI* window;

    void connectActions();
};

#endif // NAVMENUWIDGET_H
