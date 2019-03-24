#ifndef LOCKUNLOCK_H
#define LOCKUNLOCK_H

#include <QWidget>

namespace Ui {
class LockUnlock;
}

enum StateClicked{
    LOCK,UNLOCK,UNLOCK_FOR_STAKING
};


class LockUnlock : public QWidget
{
    Q_OBJECT

public:
    explicit LockUnlock(QWidget *parent = nullptr);
    ~LockUnlock();
    int lock = 0;
signals:
    void Mouse_Entered();
    void Mouse_Leave();

    void lockClicked(const StateClicked& state);
protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

public slots:
    void onLockClicked();
    void onUnlockClicked();
    void onStakingClicked();

private:
    Ui::LockUnlock *ui;
};

#endif // LOCKUNLOCK_H
