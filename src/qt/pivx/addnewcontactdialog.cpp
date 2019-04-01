#include "qt/pivx/addnewcontactdialog.h"
#include "qt/pivx/forms/ui_addnewcontactdialog.h"
#include "QGraphicsDropShadowEffect"
#include "QFile"

AddNewContactDialog::AddNewContactDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddNewContactDialog)
{
    ui->setupUi(this);


    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Title

    ui->labelTitle->setText(tr("Add Label for save it"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    ui->labelMessage->setText(tr("Set a label for the selected address"));
    ui->labelMessage->setProperty("cssClass", "text-main-grey");


    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);


    // Address

    ui->lineEditName->setPlaceholderText("Enter a name address (e.g Only Exchages)");
    ui->lineEditName->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditName->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditName->setGraphicsEffect(shadowEffect2);


    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnOk->setText("SAVE");
    ui->btnOk->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnOk, SIGNAL(clicked()), this, SLOT(accept()));
}

QString AddNewContactDialog::getLabel(){
    return ui->lineEditName->text();
}

AddNewContactDialog::~AddNewContactDialog()
{
    delete ui;
}
