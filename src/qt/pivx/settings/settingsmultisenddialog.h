// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
