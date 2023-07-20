// Copyright (c) 2011-2017 The Bitcoin developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_QT_RPCEXECUTOR_H
#define hemis_QT_RPCEXECUTOR_H

#include "rpc/server.h"

#include <QObject>
#include <QString>
#include <QTimer>

/*
 * Object for executing console RPC commands in a separate thread.
*/
class RPCExecutor : public QObject
{
    Q_OBJECT
public:
    enum CommandClass {
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

    static QString categoryClass(int category);

public Q_SLOTS:
    void request(const QString& command);

Q_SIGNALS:
    void reply(int category, const QString& command);

private:
    bool ExecuteCommandLine(std::string& strResult, const std::string& strCommand);
};


/*
 * Class for handling RPC timers
 * (used for e.g. re-locking the wallet after a timeout)
 */
class QtRPCTimerBase: public QObject, public RPCTimerBase
{
    Q_OBJECT
public:
    QtRPCTimerBase(std::function<void(void)>& _func, int64_t millis):
        func(_func)
    {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, [this]{ func(); });
        timer.start(millis);
    }
    ~QtRPCTimerBase() {}

private:
    QTimer timer;
    std::function<void(void)> func;
};

/*
 * Specialization of the RPCTimer interface
*/
class QtRPCTimerInterface: public RPCTimerInterface
{
public:
    ~QtRPCTimerInterface() {}
    const char *Name() { return "Qt"; }
    RPCTimerBase* NewTimer(std::function<void(void)>& func, int64_t millis)
    {
        return new QtRPCTimerBase(func, millis);
    }
};


#endif // hemis_QT_RPCEXECUTOR_H
