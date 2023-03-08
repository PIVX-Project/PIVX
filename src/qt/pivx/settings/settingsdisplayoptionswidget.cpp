// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsdisplayoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingsdisplayoptionswidget.h"
#include <QListView>
#include <QSettings>
#include <QDir>
#include "guiutil.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "qt/pivx/qtutils.h"

SettingsDisplayOptionsWidget::SettingsDisplayOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window,parent),
    ui(new Ui::SettingsDisplayOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title - Subtitle
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);

    ui->labelTitleLanguage->setProperty("cssClass", "text-main-settings");
    ui->labelTitleUnit->setProperty("cssClass", "text-main-settings");
    ui->labelTitleDigits->setProperty("cssClass", "text-main-settings");
    ui->labelTitleUrl->setProperty("cssClass", "text-main-settings");

    // TODO: Reconnect this option to an action. Hide it for now
    ui->labelTitleUrl->hide();

    // Switch Balance (hide for now)
    ui->pushButtonSwitchBalance->setProperty("cssClass", "btn-switch");
    ui->pushButtonSwitchBalance->setVisible(false);

    // Hide checkbox if qtcharts not used
#ifndef USE_QTCHARTS
    ui->checkBoxHideCharts->setVisible(false);
#endif

    // Combobox
    ui->comboBoxLanguage->setProperty("cssClass", "btn-combo");
    ui->comboBoxLanguage->setView(new QListView());
    ui->comboBoxLanguage->setEditable(true);
    QLineEdit* LanguageEdit = new QLineEdit(ui->comboBoxLanguage);
    LanguageEdit->setReadOnly(true);
    LanguageEdit->setAlignment(Qt::AlignRight);
    ui->comboBoxLanguage->setLineEdit(LanguageEdit);

    ui->comboBoxUnit->setProperty("cssClass", "btn-combo");
    ui->comboBoxUnit->setView(new QListView());
    ui->comboBoxUnit->setModel(new BitcoinUnits(this));
    ui->comboBoxUnit->setModelColumn(Qt::DisplayRole);
    ui->comboBoxUnit->setEditable(true);
    QLineEdit* UnitEdit = new QLineEdit(ui->comboBoxUnit);
    UnitEdit->setReadOnly(true);
    UnitEdit->setAlignment(Qt::AlignRight);
    ui->comboBoxUnit->setLineEdit(UnitEdit);

    ui->comboBoxDigits->setProperty("cssClass", "btn-combo-options");

    ui->comboBoxDigits->setView(new QListView());
    ui->comboBoxDigits->setEditable(true);
    QLineEdit* DigitsEdit = new QLineEdit(ui->comboBoxDigits);
    DigitsEdit->setReadOnly(true);
    DigitsEdit->setAlignment(Qt::AlignRight);
    ui->comboBoxDigits->setLineEdit(DigitsEdit);

    /* Number of displayed decimal digits selector */
    QString digits;
    for (int index = 2; index <= 8; index++) {
        digits.setNum(index);
        ui->comboBoxDigits->addItem(digits, digits);
    }

    // Urls
    ui->lineEditUrl->setPlaceholderText("e.g. https://example.com/tx/%s");
    initCssEditLine(ui->lineEditUrl);
    // TODO: Reconnect this option to an action. Hide it for now
    ui->lineEditUrl->hide();

    // Buttons
    ui->pushButtonSave->setText(tr("SAVE"));
    ui->pushButtonReset->setText(tr("Reset to default"));
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);
    setCssBtnSecondary(ui->pushButtonClean);

    connect(ui->pushButtonSave, &QPushButton::clicked, [this] { Q_EMIT saveSettings(); });
    connect(ui->pushButtonReset, &QPushButton::clicked, this, &SettingsDisplayOptionsWidget::onResetClicked);
    connect(ui->pushButtonClean, &QPushButton::clicked, [this] { Q_EMIT discardSettings(); });
}

void SettingsDisplayOptionsWidget::initLanguages()
{
    const QString& selectedLang = this->clientModel->getOptionsModel()->getLang();
    /* Language selector */
    QDir translations(":translations");
    QString defaultStr = QString("(") + tr("default") + QString(")");
    ui->comboBoxLanguage->addItem(defaultStr, QVariant(""));
    QStringList list = translations.entryList();
    int selectedIndex = 0;
    for (int i = 0; i < list.size(); ++i) {
        const QString& langStr = list[i];
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if (langStr.contains("_")) {
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        } else {
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        // Save selected index
        if (langStr == selectedLang) {
            selectedIndex = i + 1;
        }
    }
    ui->comboBoxLanguage->setCurrentIndex(selectedIndex);
}

void SettingsDisplayOptionsWidget::onResetClicked()
{
    if (clientModel) {
        OptionsModel *optionsModel = clientModel->getOptionsModel();
        QSettings settings;
        optionsModel->setDisplayDefaultOptions(settings, true);
        inform(tr("Options reset succeed"));
    }
}

void SettingsDisplayOptionsWidget::setMapper(QDataWidgetMapper *mapper)
{
    mapper->addMapping(ui->comboBoxDigits, OptionsModel::Digits);
    mapper->addMapping(ui->comboBoxLanguage, OptionsModel::Language, "currentData");
    mapper->addMapping(ui->comboBoxUnit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->pushButtonSwitchBalance, OptionsModel::HideZeroBalances);
#ifdef USE_QTCHARTS
    mapper->addMapping(ui->checkBoxHideCharts, OptionsModel::HideCharts);
#endif
}

void SettingsDisplayOptionsWidget::loadClientModel()
{
    if (clientModel) {
        ui->comboBoxUnit->setCurrentIndex(this->clientModel->getOptionsModel()->getDisplayUnit());
        initLanguages();
    }
}

SettingsDisplayOptionsWidget::~SettingsDisplayOptionsWidget()
{
    delete ui;
}
