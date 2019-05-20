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

    void setAddress_SM(const QString& address);
public slots:
    void on_signMessageButton_SM_clicked();
    void on_pasteButton_SM_clicked();
    void on_addressBookButton_SM_clicked();
private:
    Ui::SettingsSignMessageWidgets *ui;
};

#endif // SETTINGSSIGNMESSAGEWIDGETS_H
