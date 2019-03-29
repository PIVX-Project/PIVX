#ifndef SETTINGSPEERSLISTWIDGET_H
#define SETTINGSPEERSLISTWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsPeersListWidget;
}

class SettingsPeersListWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsPeersListWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsPeersListWidget();

private:
    Ui::SettingsPeersListWidget *ui;
};

#endif // SETTINGSPEERSLISTWIDGET_H
