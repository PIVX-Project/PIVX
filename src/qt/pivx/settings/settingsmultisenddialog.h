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

    QString getAddress();
    QString getLabel();
    int getPercentage();

    bool isOk = false;
private:
    Ui::SettingsMultisendDialog *ui;
};

#endif // SETTINGSMULTISENDDIALOG_H
