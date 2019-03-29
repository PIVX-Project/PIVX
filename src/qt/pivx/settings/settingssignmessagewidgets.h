#ifndef SETTINGSSIGNMESSAGEWIDGETS_H
#define SETTINGSSIGNMESSAGEWIDGETS_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
namespace Ui {
class SettingsSignMessageWidgets;
}

class SettingsSignMessageWidgets : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsSignMessageWidgets(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsSignMessageWidgets();

private:
    Ui::SettingsSignMessageWidgets *ui;
};

#endif // SETTINGSSIGNMESSAGEWIDGETS_H
