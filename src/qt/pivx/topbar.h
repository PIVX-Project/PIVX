#ifndef TOPBAR_H
#define TOPBAR_H

#include "lockunlock.h"

#include <QWidget>

class PIVXGUI;

namespace Ui {
class TopBar;
}

class TopBar : public QWidget
{
    Q_OBJECT

public:
    explicit TopBar(PIVXGUI* _mainWindow, QWidget *parent = nullptr);
    ~TopBar();

    void showTop();
    void showBottom();
    void showPasswordDialog();

private slots:
    void onBtnReceiveClicked();
    void onThemeClicked();
    void onBtnLockClicked();
    void lockDropdownMouseLeave();
    void lockDropdownClicked(const StateClicked&);
private:
    Ui::TopBar *ui;
    PIVXGUI* mainWindow;
    LockUnlock *lockUnlockWidget = nullptr;
    bool chkBtnLock,chkBtnUnlock, chkBtnStaking;
};

#endif // TOPBAR_H
