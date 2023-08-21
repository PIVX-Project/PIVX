// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include <QDialog>
#include "qt/hemis/prunnable.h"
#include "support/allocators/secure.h"
#include <QCheckBox>

class WalletModel;
class hemisGUI;

namespace Ui
{
class AskPassphraseDialog;
class QCheckBox;
}

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class AskPassphraseDialog : public QDialog, public Runnable
{
    Q_OBJECT

public:
    enum class Mode {
        Encrypt,         /**< Ask passphrase twice and encrypt */
        UnlockAnonymize, /**< Ask passphrase and unlock only for anonymization */
        Unlock,          /**< Ask passphrase and unlock */
        ChangePass,      /**< Ask old passphrase + new passphrase twice */
        Decrypt          /**< Ask passphrase and decrypt wallet */
    };

    // Context from where / for what the passphrase dialog was called to set the status of the checkbox
    // Partly redundant to Mode above, but offers more flexibility for future enhancements
    enum class Context {
        Unlock_Menu,    /** Unlock wallet from menu     */
        Unlock_Full,    /** Wallet needs to be fully unlocked */
        Encrypt,        /** Encrypt unencrypted wallet */
        ToggleLock,     /** Toggle wallet lock state */
        ChangePass,     /** Change passphrase */
        Send_HMS,       /** Send HMS */
        BIP_38,         /** BIP38 menu */
        Multi_Sig,      /** Multi-Signature dialog */
        Sign_Message,   /** Sign/verify message dialog */
        UI_Vote,        /** Governance Tab UI Voting */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget* parent, WalletModel* model, Context context);
    ~AskPassphraseDialog();

    void showEvent(QShowEvent *event) override;
    void accept() override;

private:
    Ui::AskPassphraseDialog* ui;
    Mode mode;
    WalletModel* model;
    Context context;
    bool fCapsLock;
    SecureString newpassCache = "";

    void updateWarningsLabel();
    void run(int type) override;
    void onError(QString error, int type) override;
    QCheckBox *btnWatch;

    void initWatch(QWidget *parent);

private Q_SLOTS:
    void onWatchClicked();
    void textChanged();
    void warningMessage();
    void errorEncryptingWallet();
    bool openStandardDialog(QString title = "", QString body = "", QString okBtn = "OK", QString cancelBtn = "");

protected:
    bool event(QEvent* event) override ;
    bool eventFilter(QObject* object, QEvent* event) override;
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
