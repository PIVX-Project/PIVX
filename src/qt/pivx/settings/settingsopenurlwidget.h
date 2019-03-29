#ifndef SETTINGSOPENURLWIDGET_H
#define SETTINGSOPENURLWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsOpenUrlWidget;
}

class SettingsOpenUrlWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsOpenUrlWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsOpenUrlWidget();

private:
    Ui::SettingsOpenUrlWidget *ui;
};

#endif // SETTINGSOPENURLWIDGET_H
