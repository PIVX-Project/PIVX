// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QThread>
#include <iostream>
#include <QTimer>
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
    void error(QString err, int type);

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
    void loadingTextChange();

private:
    Ui::LoadingDialog *ui;
    QTimer *loadingTimer = nullptr;
    int loading = 0;
};

#endif // LOADINGDIALOG_H
