// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QThread>
#include <QPointer>
#include <iostream>
#include <QTimer>
#include "qt/pivx/prunnable.h"
#include "qt/walletmodel.h"

namespace Ui {
class LoadingDialog;
}

class Worker : public QObject {
    Q_OBJECT
public:
    Worker(Runnable* runnable, int type):runnable(runnable), type(type){}
    ~Worker(){
        runnable = nullptr;
    }
    virtual void clean() {};
    void setType(int _type) { type = _type; }
public Q_SLOTS:
    void process();
Q_SIGNALS:
    void finished();
    void error(QString err, int type);

private:
    Runnable* runnable;
    int type;
};

/*
 * Worker that keeps track of the wallet unlock context
 */
class WalletWorker : public Worker {
    Q_OBJECT
public:
    WalletWorker(Runnable* runnable, int type, std::unique_ptr<WalletModel::UnlockContext> _pctx):
        Worker::Worker(runnable, type),
        pctx(std::move(_pctx))
    {}
    void clean() override
    {
        if (pctx) pctx.reset();
    }
    void setContext(std::unique_ptr<WalletModel::UnlockContext> _pctx)
    {
        clean();
        pctx = std::move(_pctx);
    }
private:
    std::unique_ptr<WalletModel::UnlockContext> pctx{nullptr};
};

class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadingDialog(QWidget *parent = nullptr, QString loadingMsg = "");
    ~LoadingDialog();

    void execute(Runnable *runnable, int type, std::unique_ptr<WalletModel::UnlockContext> pctx = nullptr);

public Q_SLOTS:
    void finished();
    void loadingTextChange();

private:
    Ui::LoadingDialog *ui;
    QTimer *loadingTimer = nullptr;
    int loading = 0;
};

#endif // LOADINGDIALOG_H
