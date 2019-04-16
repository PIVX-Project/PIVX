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
    void checkLanguage();

signals:
    void onLanguageSelected();

public slots:
    void onNextClicked();
    void onBackClicked();
    void onSkipClicked();

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
