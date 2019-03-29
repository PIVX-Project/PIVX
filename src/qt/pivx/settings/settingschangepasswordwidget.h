#ifndef SETTINGSCHANGEPASSWORDWIDGET_H
#define SETTINGSCHANGEPASSWORDWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
namespace Ui {
class SettingsChangePasswordWidget;
}

class SettingsChangePasswordWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsChangePasswordWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsChangePasswordWidget();

private:
    Ui::SettingsChangePasswordWidget *ui;
};

#endif // SETTINGSCHANGEPASSWORDWIDGET_H
