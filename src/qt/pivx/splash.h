// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPLASH_H
#define SPLASH_H

#include <QWidget>

#include <memory>

class NetworkStyle;

namespace interfaces {
    class Handler;
};

namespace Ui {
class Splash;
}

class Splash : public QWidget
{
    Q_OBJECT

public:
    explicit Splash(const NetworkStyle* networkStyle);
    ~Splash();

public Q_SLOTS:
    /** Slot to call finish() method as it's not defined as slot */
    void slotFinish(QWidget* mainWin);

    /** Show message and progress */
    void showMessage(const QString& message, int alignment, const QColor& color);

protected:
    void closeEvent(QCloseEvent* event);

private:
    Ui::Splash *ui;

    // Listeners
    std::unique_ptr<interfaces::Handler> m_handler_init_message;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;

    /** Connect core signals to splash screen */
    void subscribeToCoreSignals();
    /** Disconnect core signals to splash screen */
    void unsubscribeFromCoreSignals();
};

#endif // SPLASH_H
