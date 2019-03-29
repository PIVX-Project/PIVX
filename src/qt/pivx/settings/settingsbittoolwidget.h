#ifndef SETTINGSBITTOOLWIDGET_H
#define SETTINGSBITTOOLWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsBitToolWidget;
}

class SettingsBitToolWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsBitToolWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsBitToolWidget();

private:
    Ui::SettingsBitToolWidget *ui;
};

#endif // SETTINGSBITTOOLWIDGET_H
