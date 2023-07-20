// Copyright (c) 2019-2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WELCOMECONTENTWIDGET_H
#define WELCOMECONTENTWIDGET_H

#include <QWidget>
#include <QDialog>
#include <QPushButton>
class OptionsModel;

namespace Ui {
class WelcomeContentWidget;
}

class WelcomeContentWidget : public QDialog
{
    Q_OBJECT

public:
    explicit WelcomeContentWidget(QWidget *parent = nullptr);
    ~WelcomeContentWidget();
    int pos = 0;
    bool isOk = false;

    void setModel(OptionsModel *model);

Q_SIGNALS:
    void onLanguageSelected();

public Q_SLOTS:
    void onNextClicked();
    void onBackClicked();
    void onSkipClicked();
    void checkLanguage();

private:
    Ui::WelcomeContentWidget *ui;
    QPushButton *icConfirm1;
    QPushButton *icConfirm2;
    QPushButton *icConfirm3;
    QPushButton *icConfirm4;
    QPushButton *backButton;
    QPushButton *nextButton;

    OptionsModel *model;

    void initLanguages();
};

#endif // WELCOMECONTENTWIDGET_H
