// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPLASH_H
#define SPLASH_H

#include <QWidget>

namespace Ui {
class Splash;
}

class Splash : public QWidget
{
    Q_OBJECT

public:
    explicit Splash(QWidget *parent = nullptr);
    ~Splash();

public slots:
    /** Slot to call finish() method as it's not defined as slot */
    void slotFinish(QWidget* mainWin);

    /** Show message and progress */
    void showMessage(const QString& message, int alignment, const QColor& color);

protected:
    void closeEvent(QCloseEvent* event);

private:
    Ui::Splash *ui;

    /** Connect core signals to splash screen */
    void subscribeToCoreSignals();
    /** Disconnect core signals to splash screen */
    void unsubscribeFromCoreSignals();
};

#endif // SPLASH_H
