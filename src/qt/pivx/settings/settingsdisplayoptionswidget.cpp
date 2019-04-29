#include "qt/pivx/settings/settingsdisplayoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingsdisplayoptionswidget.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"
#include <QDir>
#include "guiutil.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include <QSettings>

SettingsDisplayOptionsWidget::SettingsDisplayOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window,parent),
    ui(new Ui::SettingsDisplayOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Display");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelTitleLanguage->setText("Language");
    ui->labelTitleLanguage->setProperty("cssClass", "text-main-grey");

    ui->labelTitleUnit->setText("Unit to show amount");
    ui->labelTitleUnit->setProperty("cssClass", "text-main-grey");

    ui->labelTitleDigits->setText("Decimal digits");
    ui->labelTitleDigits->setProperty("cssClass", "text-main-grey");

    ui->labelTitleUrl->setText("Third party transactions URLs");
    ui->labelTitleUrl->setProperty("cssClass", "text-main-grey");

    // Switch


    ui->pushButtonSwitchBalance->setText("Hide empty balances");
    ui->pushButtonSwitchBalance->setProperty("cssClass", "btn-switch");

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
    ui->comboBoxUnit->setEditable(true);
    QLineEdit* UnitEdit = new QLineEdit(ui->comboBoxUnit);
    UnitEdit->setReadOnly(true);
    UnitEdit->setAlignment(Qt::AlignRight);
    ui->comboBoxUnit->setLineEdit(UnitEdit);

    ui->comboBoxDigits->setProperty("cssClass", "btn-combo-options");

    QListView * listViewDigits = new QListView();
    ui->comboBoxDigits->setView(listViewDigits);

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

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->comboBoxDigits->setGraphicsEffect(shadowEffect);

    // Urls

    ui->lineEditUrl->setPlaceholderText("e.g. https://example.com/tx/%s");
    ui->lineEditUrl->setProperty("cssClass", "edit-primary");
    ui->lineEditUrl->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditUrl->setGraphicsEffect(shadowEffect);

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

    initLanguages();
    connect(ui->comboBoxLanguage, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
    connect(ui->comboBoxLanguage ,SIGNAL(currentIndexChanged(const QString&)),this,SLOT(languageChanged(const QString&)));
}

void SettingsDisplayOptionsWidget::initLanguages(){
    /* Language selector */
    QDir translations(":translations");
    ui->comboBoxLanguage->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    foreach (const QString& langStr, translations.entryList()) {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_")){
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        else{
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->comboBoxLanguage->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }
}

void SettingsDisplayOptionsWidget::languageChanged(const QString& newValue){
    QString sel = ui->comboBoxLanguage->currentData().toString();
    QSettings settings;
    if (settings.value("language") != sel){
        settings.setValue("language", sel);
        //emit onLanguageSelected();
    }
}

void SettingsDisplayOptionsWidget::showRestartWarning(bool fPersistent){

    // TODO: Add warning..
    /*
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if (fPersistent) {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    } else {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
    }
     */
}

void SettingsDisplayOptionsWidget::setMapper(QDataWidgetMapper *mapper){
    mapper->addMapping(ui->comboBoxDigits, OptionsModel::Digits);
    mapper->addMapping(ui->comboBoxLanguage, OptionsModel::Language);
    mapper->addMapping(ui->comboBoxUnit, OptionsModel::DisplayUnit);
    //mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
    mapper->addMapping(ui->pushButtonSwitchBalance, OptionsModel::HideZeroBalances);
    //mapper->addMapping(ui->checkBoxHideOrphans, OptionsModel::HideOrphans);

}

SettingsDisplayOptionsWidget::~SettingsDisplayOptionsWidget()
{
    delete ui;
}
