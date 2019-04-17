#ifndef SETTINGSINFORMATIONWIDGET_H
#define SETTINGSINFORMATIONWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "rpcconsole.h"

namespace Ui {
class SettingsInformationWidget;
}

class SettingsInformationWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsInformationWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsInformationWidget();

    void loadClientModel() override;

private slots:
    void setNumConnections(int count);
    void setNumBlocks(int count);
    void openNetworkMonitor();

private:
    Ui::SettingsInformationWidget *ui;
    RPCConsole *rpcConsole = nullptr;
};

#endif // SETTINGSINFORMATIONWIDGET_H
