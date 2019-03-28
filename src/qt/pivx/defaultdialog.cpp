#include "qt/pivx/defaultdialog.h"
#include "qt/pivx/forms/ui_defaultdialog.h"

DefaultDialog::DefaultDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DefaultDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Text

    ui->labelTitle->setText("Dialog Title");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

DefaultDialog::~DefaultDialog()
{
    delete ui;
}
