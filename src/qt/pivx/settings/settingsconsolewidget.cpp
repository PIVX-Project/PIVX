#include "qt/pivx/settings/settingsconsolewidget.h"
#include "qt/pivx/settings/forms/ui_settingsconsolewidget.h"
#include "QGraphicsDropShadowEffect"

SettingsConsoleWidget::SettingsConsoleWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window,parent),
    ui(new Ui::SettingsConsoleWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Console");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    // Effects

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    // Console container

    ui->consoleWidget->setProperty("cssClass", "container-square");
    ui->consoleWidget->setGraphicsEffect(shadowEffect2);

    // Edit

    ui->lineEdit->setPlaceholderText("Console input");
    ui->lineEdit->setProperty("cssClass", "edit-primary");
    ui->lineEdit->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEdit->setEchoMode(QLineEdit::Password);
    ui->lineEdit->setGraphicsEffect(shadowEffect);


    // Buttons

    ui->pushButton->setProperty("cssClass", "ic-arrow");

    ui->pushButtonCommandOptions->setText("Command Line Options ");
    ui->pushButtonCommandOptions->setProperty("cssClass", "btn-secundary");

}

SettingsConsoleWidget::~SettingsConsoleWidget()
{
    delete ui;
}
