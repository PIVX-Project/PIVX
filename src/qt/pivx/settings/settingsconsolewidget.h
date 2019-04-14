#ifndef SETTINGSCONSOLEWIDGET_H
#define SETTINGSCONSOLEWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "guiutil.h"
#include "net.h"
#include <QCompleter>

class ClientModel;
class RPCTimerInterface;

namespace Ui {
class SettingsConsoleWidget;
}

QT_BEGIN_NAMESPACE
class QMenu;
class QItemSelection;
QT_END_NAMESPACE

class SettingsConsoleWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsConsoleWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsConsoleWidget();

    void setClientModel(ClientModel* model);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

public slots:
    void clear();
    void message(int category, const QString& message, bool html = false);
    /** Go forward or back in history */
    void browseHistory(int offset);
    /** Scroll console view to end */
    void scrollToEnd();

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event);

signals:
    // For RPC command executor
    void stopExecutor();
    void cmdRequest(const QString& command);

private:
    Ui::SettingsConsoleWidget *ui;

    ClientModel* clientModel;
    QStringList history;
    int historyPtr;
    RPCTimerInterface *rpcTimerInterface;
    QCompleter *autoCompleter;

    void startExecutor();

private slots:
    void on_lineEdit_returnPressed();


};

#endif // SETTINGSCONSOLEWIDGET_H
