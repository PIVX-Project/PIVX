#include "qt/pivx/settings/settingsmainoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingsmainoptionswidget.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

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


    ui->comboBoxDbCache->setProperty("cssClass", "btn-combo-options");
    ui->comboBoxDbCache->setGraphicsEffect(shadowEffect);

    QListView * listViewDbCache = new QListView();

    ui->comboBoxDbCache->setView(listViewDbCache);

    ui->comboBoxDbCache->addItem("200");
    ui->comboBoxDbCache->addItem("100");

    ui->comboBoxThreads->setProperty("cssClass", "btn-combo-options");
    ui->comboBoxThreads->setGraphicsEffect(shadowEffect);

    QListView * listViewThreads = new QListView();
    ui->comboBoxThreads->setView(listViewThreads);

    ui->comboBoxThreads->addItem("0");
    ui->comboBoxThreads->addItem("2");

    ui->comboBoxPercentagezPiv->setProperty("cssClass", "btn-combo-options");
    ui->comboBoxPercentagezPiv->setGraphicsEffect(shadowEffect);

    QListView * listViewPercentagezPiv = new QListView();
    ui->comboBoxPercentagezPiv->setView(listViewPercentagezPiv);

    ui->comboBoxPercentagezPiv->addItem("10%");
    ui->comboBoxPercentagezPiv->addItem("20%");

    ui->comboBoxDenomzPiv->setProperty("cssClass", "btn-combo-options");
    ui->comboBoxDenomzPiv->setGraphicsEffect(shadowEffect);

    QListView * listViewDenomzPiv = new QListView();
    ui->comboBoxDenomzPiv->setView(listViewDenomzPiv);

    ui->comboBoxDenomzPiv->addItem("Any");
    ui->comboBoxDenomzPiv->addItem("100");


    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

}

SettingsMainOptionsWidget::~SettingsMainOptionsWidget()
{
    delete ui;
}
