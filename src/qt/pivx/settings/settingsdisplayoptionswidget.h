#ifndef SETTINGSDISPLAYOPTIONSWIDGET_H
#define SETTINGSDISPLAYOPTIONSWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsDisplayOptionsWidget;
}

class SettingsDisplayOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsDisplayOptionsWidget(PIVXGUI* _window = nullptr, QWidget *parent = nullptr);
    ~SettingsDisplayOptionsWidget();

    void initLanguages();

public slots:
    void showRestartWarning(bool fPersistent);
    void languageChanged(const QString& newValue);

private:
    Ui::SettingsDisplayOptionsWidget *ui;
};

#endif // SETTINGSDISPLAYOPTIONSWIDGET_H
