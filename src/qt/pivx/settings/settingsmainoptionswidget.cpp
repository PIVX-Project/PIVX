#include "qt/pivx/settings/settingsmainoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingsmainoptionswidget.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "main.h" // for MAX_SCRIPTCHECK_THREADS
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet.h" // for CWallet::minTxFee
#endif

#include <boost/thread.hpp>

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>


SettingsMainOptionsWidget::SettingsMainOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsMainOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    ui->labelDivider->setProperty("cssClass", "container-divider");

    // Title

    ui->labelTitle->setText("Main");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelTitleSizeDb->setText("Size of database cache");
    ui->labelTitleSizeDb->setProperty("cssClass", "text-main-grey");

    ui->labelTitleThreads->setText("Number of script verification threads");
    ui->labelTitleThreads->setProperty("cssClass", "text-main-grey");

    ui->labelTitlePercentagezPiv->setText("Percentage of automated zPIV");
    ui->labelTitlePercentagezPiv->setProperty("cssClass", "text-main-grey");

    ui->labelTitleDenomzPiv->setText("Preferred automint zPIV denomination");
    ui->labelTitleDenomzPiv->setProperty("cssClass", "text-main-grey");

    // Switch

    ui->pushSwitchStart->setText("Start PIVX on system login");
    ui->pushSwitchStart->setProperty("cssClass", "btn-switch");

    ui->pushSwitchAutomint->setText("Enable zPIV automint");
    ui->pushSwitchAutomint->setProperty("cssClass", "btn-switch");

    // Combobox


    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);


    ui->databaseCache->setProperty("cssClass", "btn-spin-box");
    ui->databaseCache->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->databaseCache->setGraphicsEffect(shadowEffect);
    ui->threadsScriptVerif->setProperty("cssClass", "btn-spin-box");
    ui->threadsScriptVerif->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->threadsScriptVerif->setGraphicsEffect(shadowEffect);
    ui->zeromintPercentage->setProperty("cssClass", "btn-combo-options");
    ui->zeromintPercentage->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->zeromintPercentage->setGraphicsEffect(shadowEffect);
    ui->comboBoxDenomzPiv->setProperty("cssClass", "btn-combo-options");
    QListView * listView = new QListView();
    ui->comboBoxDenomzPiv->setView(listView);

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-(int)boost::thread::hardware_concurrency());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    /* Preferred Zerocoin Denominations */
    ui->comboBoxDenomzPiv->addItem(QString(tr("Any")), QVariant("0"));
    ui->comboBoxDenomzPiv->addItem(QString("1"), QVariant("1"));
    ui->comboBoxDenomzPiv->addItem(QString("5"), QVariant("5"));
    ui->comboBoxDenomzPiv->addItem(QString("10"), QVariant("10"));
    ui->comboBoxDenomzPiv->addItem(QString("50"), QVariant("50"));
    ui->comboBoxDenomzPiv->addItem(QString("100"), QVariant("100"));
    ui->comboBoxDenomzPiv->addItem(QString("500"), QVariant("500"));
    ui->comboBoxDenomzPiv->addItem(QString("1000"), QVariant("1000"));
    ui->comboBoxDenomzPiv->addItem(QString("5000"), QVariant("5000"));

}

void SettingsMainOptionsWidget::setMapper(QDataWidgetMapper *mapper){
    mapper->addMapping(ui->pushSwitchStart, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    mapper->addMapping(ui->pushSwitchAutomint, OptionsModel::ZeromintEnable);
    mapper->addMapping(ui->zeromintPercentage, OptionsModel::ZeromintPercentage);
    mapper->addMapping(ui->comboBoxDenomzPiv, OptionsModel::ZeromintPrefDenom);
}

SettingsMainOptionsWidget::~SettingsMainOptionsWidget()
{
    delete ui;
}
