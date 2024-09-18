// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_SETTINGS_SETTINGSDISPLAYOPTIONSWIDGET_H
#define PIVX_QT_PIVX_SETTINGS_SETTINGSDISPLAYOPTIONSWIDGET_H

#include <QWidget>
#include <QDataWidgetMapper>
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

    void setMapper(QDataWidgetMapper *mapper);
    void initLanguages();
    void loadClientModel() override;

public Q_SLOTS:
    void onResetClicked();

Q_SIGNALS:
    void saveSettings();
    void discardSettings();

private:
    Ui::SettingsDisplayOptionsWidget *ui;
};

#endif // PIVX_QT_PIVX_SETTINGS_SETTINGSDISPLAYOPTIONSWIDGET_H
