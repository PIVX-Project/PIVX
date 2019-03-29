#ifndef SETTINGSMULTISENDWIDGET_H
#define SETTINGSMULTISENDWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

class PIVXGUI;

namespace Ui {
class SettingsMultisendWidget;
}

class SettingsMultisendWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsMultisendWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsMultisendWidget();

private slots:
    void onAddRecipientClicked();

private:
    Ui::SettingsMultisendWidget *ui;
    PIVXGUI* window;
};

#endif // SETTINGSMULTISENDWIDGET_H
