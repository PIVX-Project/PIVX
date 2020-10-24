// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/welcomecontentwidget.h"
#include "qt/pivx/forms/ui_welcomecontentwidget.h"

#include "guiutil.h"

#include <QDir>
#include <QFile>
#include <QListView>
#include <QScreen>
#include <QSettings>

WelcomeContentWidget::WelcomeContentWidget(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint),
    ui(new Ui::WelcomeContentWidget),
    icConfirm1(new QPushButton()),
    icConfirm2(new QPushButton()),
    icConfirm3(new QPushButton()),
    icConfirm4(new QPushButton()),
    backButton(new QPushButton()),
    nextButton(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(GUIUtil::loadStyleSheet());

    ui->frame->setProperty("cssClass", "container-welcome-stack");
#ifdef Q_OS_MAC
    ui->frame_2->load("://bg-welcome");
    ui->frame_2->setProperty("cssClass", "container-welcome-no-image");
#else
    ui->frame_2->setProperty("cssClass", "container-welcome");
#endif

    backButton = new QPushButton(ui->container);
    nextButton = new QPushButton(ui->container);

    backButton->show();
    backButton->raise();
    nextButton->show();
    nextButton->raise();

    backButton->setProperty("cssClass", "btn-welcome-back");
    nextButton->setProperty("cssClass", "btn-welcome-next");

    QSize BUTTON_SIZE = QSize(60, 60);
    backButton->setMinimumSize(BUTTON_SIZE);
    backButton->setMaximumSize(BUTTON_SIZE);

    nextButton->setMinimumSize(BUTTON_SIZE);
    nextButton->setMaximumSize(BUTTON_SIZE);

    int backX = 0;
    int backY = 240;
    int nextX = 820;
    int nextY = 240;

    // position
    backButton->move(backX, backY);
    backButton->setStyleSheet("background: url(://ic-arrow-white-left); background-repeat:no-repeat;background-position:center;border:  0;background-color:#5c4b7d;color: #5c4b7d; border-radius:2px;");
    nextButton->move(nextX, nextY);
    nextButton->setStyleSheet("background: url(://ic-arrow-white-right);background-repeat:no-repeat;background-position:center;border:  0;background-color:#5c4b7d;color: #5c4b7d; border-radius:2px;");

    if (pos == 0) {
        backButton->setVisible(false);
    }

    ui->labelLine1->setProperty("cssClass", "line-welcome");
    ui->labelLine2->setProperty("cssClass", "line-welcome");
    ui->labelLine3->setProperty("cssClass", "line-welcome");

    ui->groupBoxName->setProperty("cssClass", "container-welcome-box");
    ui->groupContainer->setProperty("cssClass", "container-welcome-box");

    ui->pushNumber1->setProperty("cssClass", "btn-welcome-number-check");
    ui->pushNumber1->setEnabled(false);
    ui->pushNumber2->setProperty("cssClass", "btn-welcome-number-check");
    ui->pushNumber2->setEnabled(false);
    ui->pushNumber3->setProperty("cssClass", "btn-welcome-number-check");
    ui->pushNumber3->setEnabled(false);
    ui->pushNumber4->setProperty("cssClass", "btn-welcome-number-check");
    ui->pushNumber4->setEnabled(false);

    ui->pushName1->setProperty("cssClass", "btn-welcome-name-check");
    ui->pushName1->setEnabled(false);
    ui->pushName2->setProperty("cssClass", "btn-welcome-name-check");
    ui->pushName2->setEnabled(false);
    ui->pushName3->setProperty("cssClass", "btn-welcome-name-check");
    ui->pushName3->setEnabled(false);
    ui->pushName4->setProperty("cssClass", "btn-welcome-name-check");
    ui->pushName4->setEnabled(false);

    ui->stackedWidget->setCurrentIndex(0);

    // Frame 1
    ui->page_1->setProperty("cssClass", "container-welcome-step1");
    ui->labelTitle1->setProperty("cssClass", "text-title-welcome");
    ui->comboBoxLanguage->setProperty("cssClass", "btn-combo-welcome");
    ui->comboBoxLanguage->setView(new QListView());

    // Frame 2
    ui->page_2->setProperty("cssClass", "container-welcome-step2");
    ui->labelTitle2->setProperty("cssClass", "text-title-welcome");
    ui->labelMessage2->setProperty("cssClass", "text-main-white");

    // Frame 3
    ui->page_3->setProperty("cssClass", "container-welcome-step3");
    ui->labelTitle3->setProperty("cssClass", "text-title-welcome");
    ui->labelMessage3->setProperty("cssClass", "text-main-white");

    // Frame 4
    ui->page_4->setProperty("cssClass", "container-welcome-step4");
    ui->labelTitle4->setProperty("cssClass", "text-title-welcome");
    ui->labelMessage4->setProperty("cssClass", "text-main-white");

    // Confirm icons
    icConfirm1 = new QPushButton(ui->layoutIcon1_2);
    icConfirm2 = new QPushButton(ui->layoutIcon2_2);
    icConfirm3 = new QPushButton(ui->layoutIcon3_2);
    icConfirm4 = new QPushButton(ui->layoutIcon4_2);

    QSize BUTTON_CONFIRM_SIZE = QSize(22, 22);
    int posX = 0;
    int posY = 0;

    icConfirm1->setProperty("cssClass", "ic-step-confirm-welcome");
    icConfirm1->setMinimumSize(BUTTON_CONFIRM_SIZE);
    icConfirm1->setMaximumSize(BUTTON_CONFIRM_SIZE);
    icConfirm1->show();
    icConfirm1->raise();
    icConfirm1->setVisible(false);
    icConfirm1->move(posX, posY);
    icConfirm2->setProperty("cssClass", "ic-step-confirm-welcome");
    icConfirm2->setMinimumSize(BUTTON_CONFIRM_SIZE);
    icConfirm2->setMaximumSize(BUTTON_CONFIRM_SIZE);
    icConfirm2->move(posX, posY);
    icConfirm2->show();
    icConfirm2->raise();
    icConfirm2->setVisible(false);
    icConfirm3->setProperty("cssClass", "ic-step-confirm-welcome");
    icConfirm3->setMinimumSize(BUTTON_CONFIRM_SIZE);
    icConfirm3->setMaximumSize(BUTTON_CONFIRM_SIZE);
    icConfirm3->move(posX, posY);
    icConfirm3->show();
    icConfirm3->raise();
    icConfirm3->setVisible(false);
    icConfirm4->setProperty("cssClass", "ic-step-confirm-welcome");
    icConfirm4->setMinimumSize(BUTTON_CONFIRM_SIZE);
    icConfirm4->setMaximumSize(BUTTON_CONFIRM_SIZE);
    icConfirm4->move(posX, posY);
    icConfirm4->show();
    icConfirm4->raise();
    icConfirm4->setVisible(false);

    ui->pushButtonSkip->setProperty("cssClass", "btn-close-white");
    onNextClicked();

    connect(ui->pushButtonSkip, &QPushButton::clicked, this, &WelcomeContentWidget::close);
    connect(nextButton, &QPushButton::clicked, this, &WelcomeContentWidget::onNextClicked);
    connect(backButton, &QPushButton::clicked, this, &WelcomeContentWidget::onBackClicked);
    connect(ui->comboBoxLanguage, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &WelcomeContentWidget::checkLanguage);
    initLanguages();

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), size());
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());
}

void WelcomeContentWidget::initLanguages()
{
    /* Language selector */
    QDir translations(":translations");
    ui->comboBoxLanguage->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString& langStr : translations.entryList()) {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if (langStr.contains("_")) {
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        } else {
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }
}

void WelcomeContentWidget::setModel(OptionsModel *model)
{
    this->model = model;
}

void WelcomeContentWidget::checkLanguage()
{
    QString sel = ui->comboBoxLanguage->currentData().toString();
    QSettings settings;
    if (settings.value("language") != sel){
        settings.setValue("language", sel);
        settings.sync();
        Q_EMIT onLanguageSelected();
        ui->retranslateUi(this);
    }
}

void WelcomeContentWidget::onNextClicked()
{
    switch(pos){
        case 0:{
            ui->stackedWidget->setCurrentIndex(1);
            break;
        }
        case 1:{
            backButton->setVisible(true);
            ui->stackedWidget->setCurrentIndex(2);
            ui->pushNumber2->setChecked(true);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(true);
            break;
        }
        case 2:{
            ui->stackedWidget->setCurrentIndex(3);
            ui->pushNumber3->setChecked(true);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm2->setVisible(true);
            break;
        }
        case 3:{
            ui->stackedWidget->setCurrentIndex(4);
            ui->pushNumber4->setChecked(true);
            ui->pushName4->setChecked(true);
            ui->pushName3->setChecked(true);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm3->setVisible(true);
            break;
        }
        case 4:{
            isOk = true;
            accept();
            break;
        }
    }
    pos++;

}

void WelcomeContentWidget::onBackClicked()
{
    if (pos == 0) return;
    pos--;
    switch(pos){
        case 0:{
            ui->stackedWidget->setCurrentIndex(0);
            break;
        }
        case 1:{
            ui->stackedWidget->setCurrentIndex(1);
            ui->pushNumber1->setChecked(true);
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(false);
            ui->pushNumber2->setChecked(false);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(false);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(false);
            backButton->setVisible(false);

            break;
        }
        case 2:{
            ui->stackedWidget->setCurrentIndex(2);
            ui->pushNumber2->setChecked(true);
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(false);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm2->setVisible(false);
            break;
        }
        case 3:{
            ui->stackedWidget->setCurrentIndex(3);
            ui->pushNumber3->setChecked(true);
            ui->pushNumber4->setChecked(false);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm3->setVisible(false);
            break;
        }

    }

    if (pos == 0) {
        backButton->setVisible(false);
    }
}

void WelcomeContentWidget::onSkipClicked()
{
    isOk = true;
    accept();
}

WelcomeContentWidget::~WelcomeContentWidget()
{
    delete ui;
}
