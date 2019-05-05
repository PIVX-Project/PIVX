#ifndef TOPBAR_H
#define TOPBAR_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/lockunlock.h"
#include "amount.h"
#include <QTimer>
#include <QProgressBar>

class PIVXGUI;
class WalletModel;
class ClientModel;

namespace Ui {
class TopBar;
}

class TopBar : public PWidget
{
    Q_OBJECT

public:
    explicit TopBar(PIVXGUI* _mainWindow, QWidget *parent = nullptr);
    ~TopBar();

    void showTop();
    void showBottom();
    void showPasswordDialog();

    void loadWalletModel() override;
    void loadClientModel() override;

    void encryptWallet();
public slots:
    void updateBalances(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                        const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                        const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);
    void updateDisplayUnit();

    void setNumConnections(int count);
    void setNumBlocks(int count);
    void updateAutoMintStatus();
    void updateStakingStatus();
    /** Show progress dialog e.g. for verifychain */
    void showProgress(const QString& title, int nProgress);

signals:
    // Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style);

private slots:
    void onBtnReceiveClicked();
    void onThemeClicked();
    void onBtnLockClicked();
    void lockDropdownMouseLeave();
    void lockDropdownClicked(const StateClicked&);
    void refreshStatus();
private:
    Ui::TopBar *ui;
    LockUnlock *lockUnlockWidget = nullptr;
    QProgressBar* progressBar = nullptr;

    int nDisplayUnit = -1;
    QTimer* timerStakingIcon = nullptr;

};

#endif // TOPBAR_H
