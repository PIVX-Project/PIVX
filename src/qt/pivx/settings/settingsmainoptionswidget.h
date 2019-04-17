#ifndef SETTINGSMAINOPTIONSWIDGET_H
#define SETTINGSMAINOPTIONSWIDGET_H

#include "qt/pivx/pwidget.h"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
class SettingsMainOptionsWidget;
}

class SettingsMainOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsMainOptionsWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsMainOptionsWidget();

    void setMapper(QDataWidgetMapper *mapper);

private:
    Ui::SettingsMainOptionsWidget *ui;
};

#endif // SETTINGSMAINOPTIONSWIDGET_H
