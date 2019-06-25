#include "qt/pivx/settings/settingswidget.h"
#include "qt/pivx/settings/forms/ui_settingswidget.h"
#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/settingsbittoolwidget.h"
#include "qt/pivx/settings/settingswalletrepairwidget.h"
#include "qt/pivx/settings/settingsnetworkwidget.h"
#include "qt/pivx/settings/settingswalletoptionswidget.h"
#include "qt/pivx/settings/settingsmainoptionswidget.h"
#include "qt/pivx/settings/settingsdisplayoptionswidget.h"
#include "qt/pivx/settings/settingsmultisendwidget.h"
#include "qt/pivx/settings/settingsinformationwidget.h"
#include "qt/pivx/settings/settingsconsolewidget.h"
#include "qt/pivx/qtutils.h"
#include "optionsmodel.h"
#include "clientmodel.h"
#include "utilitydialog.h"
#include <QScrollBar>
#include <QDataWidgetMapper>

SettingsWidget::SettingsWidget(PIVXGUI* parent) :
    PWidget(parent),
    ui(new Ui::SettingsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->scrollArea->setProperty("cssClass", "container");
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    ui->verticalLayout->setAlignment(Qt::AlignTop);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    ui->labelTitle->setText(tr("Settings"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    ui->pushButtonFile->setProperty("cssClass", "btn-settings-check");
    ui->pushButtonFile2->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonFile3->setProperty("cssClass", "btn-settings-options");

    ui->pushButtonConfiguration->setProperty("cssClass", "btn-settings-check");
    ui->pushButtonConfiguration3->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonConfiguration4->setProperty("cssClass", "btn-settings-options");

    ui->pushButtonOptions->setProperty("cssClass", "btn-settings-check");
    ui->pushButtonOptions1->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonOptions2->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonOptions3->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonOptions5->setProperty("cssClass", "btn-settings-options");

    ui->pushButtonTools->setProperty("cssClass", "btn-settings-check");
    ui->pushButtonTools1->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonTools2->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonTools5->setProperty("cssClass", "btn-settings-options");

    ui->pushButtonHelp->setProperty("cssClass", "btn-settings-check");
    ui->pushButtonHelp1->setProperty("cssClass", "btn-settings-options");
    ui->pushButtonHelp2->setProperty("cssClass", "btn-settings-options");

    options = {
        ui->pushButtonFile2,
        ui->pushButtonFile3,
        ui->pushButtonOptions1,
        ui->pushButtonOptions2,
        ui->pushButtonOptions3,
        ui->pushButtonOptions5,
        ui->pushButtonConfiguration3,
        ui->pushButtonConfiguration4,
        ui->pushButtonHelp2,
        ui->pushButtonTools1,
        ui->pushButtonTools2,
        ui->pushButtonTools5,
    };

    ui->pushButtonFile->setChecked(true);
    ui->fileButtonsWidget->setVisible(true);
    ui->optionsButtonsWidget->setVisible(false);
    ui->configurationButtonsWidget->setVisible(false);
    ui->toolsButtonsWidget->setVisible(false);
    ui->helpButtonsWidget->setVisible(false);

    ui->pushButtonFile2->setChecked(true);

    settingsBackupWallet = new SettingsBackupWallet(window, this);
    settingsBitToolWidget = new SettingsBitToolWidget(window, this);
    settingsSingMessageWidgets = new SettingsSignMessageWidgets(window, this);
    settingsWalletRepairWidget = new SettingsWalletRepairWidget(window, this);
    settingsNetworkWidget = new SettingsNetworkWidget(window, this);
    settingsWalletOptionsWidget = new SettingsWalletOptionsWidget(window, this);
    settingsMainOptionsWidget = new SettingsMainOptionsWidget(window, this);
    settingsDisplayOptionsWidget = new SettingsDisplayOptionsWidget(window, this);
    settingsMultisendWidget = new SettingsMultisendWidget(window, this);
    settingsInformationWidget = new SettingsInformationWidget(window, this);
    settingsConsoleWidget = new SettingsConsoleWidget(window, this);

    ui->stackedWidgetContainer->addWidget(settingsBackupWallet);
    ui->stackedWidgetContainer->addWidget(settingsBitToolWidget);
    ui->stackedWidgetContainer->addWidget(settingsSingMessageWidgets);
    ui->stackedWidgetContainer->addWidget(settingsWalletRepairWidget);
    ui->stackedWidgetContainer->addWidget(settingsNetworkWidget);
    ui->stackedWidgetContainer->addWidget(settingsWalletOptionsWidget);
    ui->stackedWidgetContainer->addWidget(settingsMainOptionsWidget);
    ui->stackedWidgetContainer->addWidget(settingsDisplayOptionsWidget);
    ui->stackedWidgetContainer->addWidget(settingsMultisendWidget);
    ui->stackedWidgetContainer->addWidget(settingsInformationWidget);
    ui->stackedWidgetContainer->addWidget(settingsConsoleWidget);
    ui->stackedWidgetContainer->setCurrentWidget(settingsBackupWallet);

    // File Section
    connect(ui->pushButtonFile, SIGNAL(clicked()), this, SLOT(onFileClicked()));
    connect(ui->pushButtonFile2, SIGNAL(clicked()), this, SLOT(onBackupWalletClicked()));
    connect(ui->pushButtonFile3, SIGNAL(clicked()), this, SLOT(onMultisendClicked()));

    // Options
    connect(ui->pushButtonOptions, SIGNAL(clicked()), this, SLOT(onOptionsClicked()));
    connect(ui->pushButtonOptions1, SIGNAL(clicked()), this, SLOT(onMainOptionsClicked()));
    connect(ui->pushButtonOptions2, SIGNAL(clicked()), this, SLOT(onWalletOptionsClicked()));
    connect(ui->pushButtonOptions3, SIGNAL(clicked()), this, SLOT(onNetworkOptionsClicked()));
    connect(ui->pushButtonOptions5, SIGNAL(clicked()), this, SLOT(onDisplayOptionsClicked()));

    // Configuration
    connect(ui->pushButtonConfiguration, SIGNAL(clicked()), this, SLOT(onConfigurationClicked()));
    connect(ui->pushButtonConfiguration3, SIGNAL(clicked()), this, SLOT(onBipToolClicked()));
    connect(ui->pushButtonConfiguration4, SIGNAL(clicked()), this, SLOT(onSignMessageClicked()));

    // Tools
    connect(ui->pushButtonTools, SIGNAL(clicked()), this, SLOT(onToolsClicked()));
    connect(ui->pushButtonTools1, SIGNAL(clicked()), this, SLOT(onInformationClicked()));
    connect(ui->pushButtonTools2, SIGNAL(clicked()), this, SLOT(onDebugConsoleClicked()));
    ui->pushButtonTools2->setShortcut(QKeySequence(SHORT_KEY + Qt::Key_C));
    connect(ui->pushButtonTools5, SIGNAL(clicked()), this, SLOT(onWalletRepairClicked()));

    // Help
    connect(ui->pushButtonHelp, SIGNAL(clicked()), this, SLOT(onHelpClicked()));
    connect(ui->pushButtonHelp1, SIGNAL(clicked()), window, SLOT(openFAQ()));
    connect(ui->pushButtonHelp2, SIGNAL(clicked()), this, SLOT(onAboutClicked()));
    

    // Get restart command-line parameters and handle restart
    connect(settingsWalletRepairWidget, &SettingsWalletRepairWidget::handleRestart, [this](QStringList arg){emit handleRestart(arg);});

    connect(settingsBackupWallet,&SettingsBackupWallet::message,this, &SettingsWidget::message);
    connect(settingsBackupWallet, &SettingsBackupWallet::showHide, this, &SettingsWidget::showHide);
    connect(settingsBackupWallet, &SettingsBackupWallet::execDialog, this, &SettingsWidget::execDialog);

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);
}

void SettingsWidget::loadClientModel(){
    this->settingsInformationWidget->setClientModel(this->clientModel);
    this->settingsConsoleWidget->setClientModel(this->clientModel);

    OptionsModel *optionsModel = this->clientModel->getOptionsModel();
    if(optionsModel) {
        mapper->setModel(optionsModel);
        setMapper();
        mapper->toFirst();
        /* keep consistency for action triggered elsewhere */
        connect(optionsModel, SIGNAL(hideOrphansChanged(bool)), this, SLOT(updateHideOrphans(bool)));

        // TODO: Connect show restart needed and apply changes.
    }
}

void SettingsWidget::loadWalletModel(){
    this->settingsBackupWallet->setWalletModel(this->walletModel);
    this->settingsSingMessageWidgets->setWalletModel(this->walletModel);
    this->settingsBitToolWidget->setWalletModel(this->walletModel);
}


void SettingsWidget::onFileClicked() {
    if (ui->pushButtonFile->isChecked()) {
        ui->fileButtonsWidget->setVisible(true);

        ui->optionsButtonsWidget->setVisible(false);
        ui->toolsButtonsWidget->setVisible(false);
        ui->configurationButtonsWidget->setVisible(false);
        ui->helpButtonsWidget->setVisible(false);
        ui->pushButtonOptions->setChecked(false);
        ui->pushButtonTools->setChecked(false);
        ui->pushButtonConfiguration->setChecked(false);
        ui->pushButtonHelp->setChecked(false);

    } else {
        ui->fileButtonsWidget->setVisible(false);
    }
}

void SettingsWidget::onBackupWalletClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsBackupWallet);
    selectOption(ui->pushButtonFile2);
}

void SettingsWidget::onSignMessageClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsSingMessageWidgets);
    selectOption(ui->pushButtonConfiguration4);
}

void SettingsWidget::onConfigurationClicked() {
    if (ui->pushButtonConfiguration->isChecked()) {
        ui->configurationButtonsWidget->setVisible(true);

        ui->optionsButtonsWidget->setVisible(false);
        ui->toolsButtonsWidget->setVisible(false);
        ui->fileButtonsWidget->setVisible(false);
        ui->helpButtonsWidget->setVisible(false);
        ui->pushButtonOptions->setChecked(false);
        ui->pushButtonTools->setChecked(false);
        ui->pushButtonFile->setChecked(false);
        ui->pushButtonHelp->setChecked(false);

    } else {
        ui->configurationButtonsWidget->setVisible(false);
    }
}

void SettingsWidget::onBipToolClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsBitToolWidget);
    selectOption(ui->pushButtonConfiguration3);
}

void SettingsWidget::onMultisendClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsMultisendWidget);
    selectOption(ui->pushButtonFile3);
}

void SettingsWidget::onOptionsClicked() {
    if (ui->pushButtonOptions->isChecked()) {
        ui->optionsButtonsWidget->setVisible(true);

        ui->fileButtonsWidget->setVisible(false);
        ui->toolsButtonsWidget->setVisible(false);
        ui->configurationButtonsWidget->setVisible(false);
        ui->helpButtonsWidget->setVisible(false);
        ui->pushButtonFile->setChecked(false);
        ui->pushButtonTools->setChecked(false);
        ui->pushButtonConfiguration->setChecked(false);
        ui->pushButtonHelp->setChecked(false);

    } else {
        ui->optionsButtonsWidget->setVisible(false);
    }
}

void SettingsWidget::onMainOptionsClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsMainOptionsWidget);
    selectOption(ui->pushButtonOptions1);
}

void SettingsWidget::onWalletOptionsClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsWalletOptionsWidget);
    selectOption(ui->pushButtonOptions2);
}

void SettingsWidget::onNetworkOptionsClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsNetworkWidget);
    selectOption(ui->pushButtonOptions3);
}

void SettingsWidget::onDisplayOptionsClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsDisplayOptionsWidget);
    selectOption(ui->pushButtonOptions5);
}


void SettingsWidget::onToolsClicked() {
    if (ui->pushButtonTools->isChecked()) {
        ui->toolsButtonsWidget->setVisible(true);

        ui->optionsButtonsWidget->setVisible(false);
        ui->fileButtonsWidget->setVisible(false);
        ui->configurationButtonsWidget->setVisible(false);
        ui->helpButtonsWidget->setVisible(false);
        ui->pushButtonOptions->setChecked(false);
        ui->pushButtonFile->setChecked(false);
        ui->pushButtonConfiguration->setChecked(false);
        ui->pushButtonHelp->setChecked(false);

    } else {
        ui->toolsButtonsWidget->setVisible(false);
    }
}

void SettingsWidget::onInformationClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsInformationWidget);
    selectOption(ui->pushButtonTools1);
}

void SettingsWidget::showDebugConsole(){
    ui->pushButtonTools->setChecked(true);
    onToolsClicked();
    ui->pushButtonTools2->setChecked(true);
    onDebugConsoleClicked();
}

void SettingsWidget::onDebugConsoleClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsConsoleWidget);
    selectOption(ui->pushButtonTools2);
}

void SettingsWidget::onWalletRepairClicked() {
    ui->stackedWidgetContainer->setCurrentWidget(settingsWalletRepairWidget);
    selectOption(ui->pushButtonTools5);
}


void SettingsWidget::onHelpClicked() {
    if (ui->pushButtonHelp->isChecked()) {
        ui->helpButtonsWidget->setVisible(true);

        ui->toolsButtonsWidget->setVisible(false);
        ui->optionsButtonsWidget->setVisible(false);
        ui->fileButtonsWidget->setVisible(false);
        ui->configurationButtonsWidget->setVisible(false);
        ui->pushButtonOptions->setChecked(false);
        ui->pushButtonFile->setChecked(false);
        ui->pushButtonConfiguration->setChecked(false);
        ui->pushButtonTools->setChecked(false);
    } else {
        ui->helpButtonsWidget->setVisible(false);
    }
}

void SettingsWidget::onAboutClicked() {
    if (!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();

}

void SettingsWidget::selectOption(QPushButton* option){
    for (QPushButton* wid : options) {
        if(wid) wid->setChecked(wid == option);
    }
}

void SettingsWidget::setMapper(){
    settingsMainOptionsWidget->setMapper(mapper);
    settingsWalletOptionsWidget->setMapper(mapper);
    settingsNetworkWidget->setMapper(mapper);
    settingsDisplayOptionsWidget->setMapper(mapper);
}

SettingsWidget::~SettingsWidget(){
    delete ui;
}
