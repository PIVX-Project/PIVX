#include "qt/pivx/loadingdialog.h"
#include "qt/pivx/forms/ui_loadingdialog.h"
#include <QMovie>

void Worker::process(){
    try {
        if (runnable)
            runnable->run(type);
        emit finished();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        emit error(QString::fromStdString(e.what()));
    }
};

LoadingDialog::LoadingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadingDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    ui->frame->setProperty("cssClass", "container-loading");

    QMovie *movie = new QMovie("://ani-loading-dark");
    ui->labelMovie->setText("");
    ui->labelMovie->setMovie(movie);
    movie->start();

    ui->labelMessage->setText("Loading");
    ui->labelMessage->setProperty("cssClass", "text-loading");
}

void LoadingDialog::execute(Runnable *runnable, int type){
    QThread* thread = new QThread;
    Worker* worker = new Worker(runnable, type);
    worker->moveToThread(thread);
    connect(worker, SIGNAL (error(QString)), this, SLOT (errorString(QString)));
    connect(thread, SIGNAL (started()), worker, SLOT (process()));
    connect(worker, SIGNAL (finished()), thread, SLOT (quit()));
    connect(worker, SIGNAL (finished()), worker, SLOT (deleteLater()));
    connect(thread, SIGNAL (finished()), thread, SLOT (deleteLater()));
    connect(worker, SIGNAL (finished()), this, SLOT (finished()));
    thread->start();
}

void LoadingDialog::finished(){
    accept();
    deleteLater();
}

LoadingDialog::~LoadingDialog()
{
    delete ui;
}
