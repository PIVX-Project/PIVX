#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QThread>
#include <iostream>
#include "qt/pivx/prunnable.h"

namespace Ui {
class LoadingDialog;
}

class Worker : public QObject {
    Q_OBJECT
public:
    Worker(Runnable* runnable, int type):runnable(runnable), type(type){}
    ~Worker(){}
public slots:
    void process();
signals:
    void finished();
    void error(QString err);

private:
    Runnable* runnable;
    int type;
};

class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();


    void execute(Runnable *runnable, int type);

public slots:
    void finished();
private:
    Ui::LoadingDialog *ui;
};

#endif // LOADINGDIALOG_H
