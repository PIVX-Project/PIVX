#include "qt/pivx/loadingdialog.h"
#include "qt/pivx/forms/ui_loadingdialog.h"
#include <QMovie>

LoadingDialog::LoadingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadingDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    ui->frame->setProperty("cssClass", "container-loading");

    QMovie *movie = new QMovie("://ani-loading");
    //QMovie *movie = new QMovie(":/res/img/dark/ani-loading.gif");
    ui->labelMovie->setText("");
    ui->labelMovie->setMovie(movie);
    movie->start();

    ui->labelMessage->setText("Loading");
    ui->labelMessage->setProperty("cssClass", "text-loading");
}

LoadingDialog::~LoadingDialog()
{
    delete ui;
}
