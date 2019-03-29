#ifndef SETTINGSWINDOWOPTIONSWIDGET_H
#define SETTINGSWINDOWOPTIONSWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsWindowOptionsWidget;
}

class SettingsWindowOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWindowOptionsWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsWindowOptionsWidget();

private:
    Ui::SettingsWindowOptionsWidget *ui;
};

#endif // SETTINGSWINDOWOPTIONSWIDGET_H
