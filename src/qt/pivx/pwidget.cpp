// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/pwidget.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/loadingdialog.h"
#include <QRunnable>
#include <QThreadPool>

PWidget::PWidget(PIVXGUI* _window, QWidget *parent) : QWidget((parent) ? parent : _window), window(_window) { init(); }
PWidget::PWidget(PWidget* parent) : QWidget(parent), window(parent->getWindow()) { init(); }

void PWidget::init()
{
    if (window)
        connect(window, &PIVXGUI::themeChanged, this, &PWidget::onChangeTheme);
}

void PWidget::setClientModel(ClientModel* model)
{
    this->clientModel = model;
    loadClientModel();
}

void PWidget::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
    loadWalletModel();
}

void PWidget::onChangeTheme(bool isLightTheme, QString& theme)
{
    this->setStyleSheet(theme);
    changeTheme(isLightTheme, theme);
    updateStyle(this);
}

void PWidget::showHideOp(bool show)
{
    Q_EMIT showHide(show);
}

void PWidget::inform(const QString& message)
{
    emitMessage("", message, CClientUIInterface::MSG_INFORMATION_SNACK);
}

void PWidget::warn(const QString& title, const QString& message)
{
    emitMessage(title, message, CClientUIInterface::MSG_ERROR);
}

bool PWidget::ask(const QString& title, const QString& message)
{
    bool ret = false;
    emitMessage(title, message, CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL, &ret);
    return ret;
}

void PWidget::showDialog(QDialog *dlg, int xDiv, int yDiv)
{
    Q_EMIT execDialog(dlg, xDiv, yDiv);
}

void PWidget::emitMessage(const QString& title, const QString& body, unsigned int style, bool* ret)
{
    Q_EMIT message(title, body, style, ret);
}

class WorkerTask : public QRunnable
{
public:
    WorkerTask(QPointer<Worker> worker) {
        this->worker = worker;
    }

    ~WorkerTask() {
        if (!worker.isNull()) worker.clear();
    }

    void run() override {
        if (!worker.isNull()) {
            Worker* _worker = worker.data();
            _worker->process();
            _worker->clean();
        }
    }

    QPointer<Worker> worker;
};

bool PWidget::execute(int type, std::unique_ptr<WalletModel::UnlockContext> pctx)
{
    if (task.isNull()) {
        Worker* worker = (!pctx) ? new Worker(this, type) : new WalletWorker(this, type, std::move(pctx));
        connect(worker, &Worker::error, this, &PWidget::errorString);

        WorkerTask* workerTask = new WorkerTask(QPointer<Worker>(worker));
        workerTask->setAutoDelete(false);
        task = QSharedPointer<WorkerTask>(workerTask);
    } else if (pctx){
        if (task->worker.isNull() || !task->worker.data()) // Must never happen
            throw std::runtime_error("Worker task null");

        // Update context
        if (dynamic_cast<WalletWorker*>(task->worker.data()) != nullptr) {
            WalletWorker* _worker = static_cast<WalletWorker*>(task->worker.data());
            _worker->setContext(std::move(pctx));
        }
        // Update type
        task->worker->setType(type);
    }
    QThreadPool::globalInstance()->start(task.data());
    return true;
}

void PWidget::errorString(QString error, int type)
{
    onError(error, type);
}


////////////////////////////////////////////////////////////////
//////////////////Override methods//////////////////////////////
////////////////////////////////////////////////////////////////


void PWidget::loadClientModel() { /* override*/ }
void PWidget::loadWalletModel() { /* override*/ }
void PWidget::changeTheme(bool isLightTheme, QString& theme) { /* override*/ }
void PWidget::run(int type) { /* override*/ }
void PWidget::onError(QString error, int type) { /* override*/ }
