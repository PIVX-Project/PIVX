#include "qt/pivx/settings/settingswalletoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingswalletoptionswidget.h"
#include <QListView>
#include "optionsmodel.h"
#include "qt/pivx/qtutils.h"

SettingsWalletOptionsWidget::SettingsWalletOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWalletOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setText(tr("Wallet"));
    ui->labelSubtitle1->setText(tr("Customize the internal wallet options"));
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Combobox
    ui->labelTitleStake->setText(tr("Stake split threshold:"));
    ui->labelTitleStake->setProperty("cssClass", "text-title");

    ui->spinBoxStakeSplitThreshold->setProperty("cssClass", "btn-spin-box");
    ui->spinBoxStakeSplitThreshold->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->spinBoxStakeSplitThreshold);

    // Radio buttons
    ui->labelTitleExpert->setText(tr("Expert"));
    ui->labelTitleExpert->setProperty("cssClass", "text-title");

    ui->radioButtonShow->setText(tr("Show Masternodes tab"));
    ui->radioButtonShow->setVisible(false);
    ui->radioButtonSpend->setText(tr("Spend unconfirmed change"));

    // Buttons
    ui->pushButtonSave->setText(tr("SAVE"));
    ui->pushButtonReset->setText(tr("Reset to default"));
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);
}

void SettingsWalletOptionsWidget::setMapper(QDataWidgetMapper *mapper){
    mapper->addMapping(ui->radioButtonSpend, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->spinBoxStakeSplitThreshold, OptionsModel::StakeSplitThreshold);
}

SettingsWalletOptionsWidget::~SettingsWalletOptionsWidget()
{
    delete ui;
}
