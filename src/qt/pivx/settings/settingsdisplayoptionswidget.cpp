#include "qt/pivx/settings/settingsdisplayoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingsdisplayoptionswidget.h"
#include "QGraphicsDropShadowEffect"
#include "QListView"

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

    ui->labelTitleTheme->setText("Interface Theme");
    ui->labelTitleTheme->setProperty("cssClass", "text-main-grey");

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

    QListView * listViewLanguage = new QListView();

    ui->comboBoxLanguage->addItem("English");
    ui->comboBoxLanguage->addItem("Spanish");

    ui->comboBoxLanguage->setView(listViewLanguage);

    ui->comboBoxTheme->setProperty("cssClass", "btn-combo");

    QListView * listViewTheme = new QListView();

    ui->comboBoxTheme->addItem("Light");
    ui->comboBoxTheme->addItem("Dark");
    ui->comboBoxTheme->setView(listViewTheme);

    ui->comboBoxUnit->setProperty("cssClass", "btn-combo");

    QListView * listViewUnit = new QListView();

    ui->comboBoxUnit->addItem("10%");
    ui->comboBoxUnit->addItem("20%");
    ui->comboBoxUnit->setView(listViewUnit);

    ui->comboBoxDigits->setProperty("cssClass", "btn-combo-options");

    QListView * listViewDigits = new QListView();

    ui->comboBoxDigits->addItem("Any");
    ui->comboBoxDigits->addItem("100");
    ui->comboBoxDigits->setView(listViewDigits);

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->comboBoxDigits->setGraphicsEffect(shadowEffect);

    // Urls

    QGraphicsDropShadowEffect* shadowEffect3 = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect3->setBlurRadius(6);

    ui->lineEditUrl->setPlaceholderText("e.g. https://example.com/tx/%s");
    ui->lineEditUrl->setProperty("cssClass", "edit-primary");
    ui->lineEditUrl->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditUrl->setGraphicsEffect(shadowEffect3);



    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

}

SettingsDisplayOptionsWidget::~SettingsDisplayOptionsWidget()
{
    delete ui;
}
