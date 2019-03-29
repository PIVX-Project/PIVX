#ifndef SETTINGSMULTISENDDIALOG_H
#define SETTINGSMULTISENDDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsMultisendDialog;
}

class SettingsMultisendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsMultisendDialog(QWidget *parent = nullptr);
    ~SettingsMultisendDialog();

private:
    Ui::SettingsMultisendDialog *ui;
};

#endif // SETTINGSMULTISENDDIALOG_H
