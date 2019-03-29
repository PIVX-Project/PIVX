#ifndef SETTINGSNETWORKMONITORWIDGET_H
#define SETTINGSNETWORKMONITORWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsNetworkMonitorWidget;
}

class SettingsNetworkMonitorWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsNetworkMonitorWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsNetworkMonitorWidget();

private:
    Ui::SettingsNetworkMonitorWidget *ui;
};

#endif // SETTINGSNETWORKMONITORWIDGET_H
