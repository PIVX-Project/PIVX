// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/hemis-config.h"
#endif

#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"
#include <QGraphicsDropShadowEffect>

#include "guiconstants.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "qt/hemis/qtutils.h"
#include "qt/hemis/loadingdialog.h"
#include "qt/hemis/defaultdialog.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

AskPassphraseDialog::AskPassphraseDialog(Mode mode, QWidget* parent, WalletModel* model, Context context) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                                                                            ui(new Ui::AskPassphraseDialog),
                                                                                                            mode(mode),
                                                                                                            model(model),
                                                                                                            context(context),
                                                                                                            fCapsLock(false),
                                                                                                            btnWatch(new QCheckBox())
{
    ui->setupUi(this);
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    ui->left->setProperty("cssClass", "container-dialog");

    ui->labelTitle->setText("Change passphrase");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->warningLabel->setProperty("cssClass", "text-subtitle");

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->pushButtonOk->setText("OK");
    ui->pushButtonOk->setProperty("cssClass", "btn-primary");

    initCssEditLine(ui->passEdit1);
    initCssEditLine(ui->passEdit2);
    initCssEditLine(ui->passEdit3);

    ui->passLabel1->setText("Current passphrase");
    ui->passLabel1->setProperty("cssClass", "text-title");

    ui->passLabel2->setText("New passphrase");
    ui->passLabel2->setProperty("cssClass", "text-title");

    ui->passLabel3->setText("Repeat passphrase");
    ui->passLabel3->setProperty("cssClass", "text-title");

    setCssProperty(ui->passWarningLabel, "text-warning-small");
    ui->passWarningLabel->setVisible(false);

    ui->passEdit1->setMinimumSize(ui->passEdit1->sizeHint());
    ui->passEdit2->setMinimumSize(ui->passEdit2->sizeHint());
    ui->passEdit3->setMinimumSize(ui->passEdit3->sizeHint());

    ui->passEdit1->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSPHRASE_SIZE);

    setShadow(ui->layoutEdit);
    setShadow(ui->layoutEdit2);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    this->model = model;

    QString title;
    switch (mode) {
    case Mode::Encrypt: // Ask passphrase x2
        ui->warningLabel->setText(tr("Enter the new passphrase to the wallet.<br/>Please use a passphrase of <b>ten or more random characters</b>, or <b>eight or more words</b>."));
        ui->passLabel1->hide();
        ui->passEdit1->hide();
        ui->layoutEdit->hide();
        title = tr("Encrypt wallet");
        initWatch(ui->layoutEdit2);
        break;
    case Mode::UnlockAnonymize:
        ui->warningLabel->setText(tr("This operation needs your wallet passphrase to unlock the wallet."));
        ui->passLabel2->hide();
        ui->passEdit2->hide();
        ui->layoutEdit2->hide();
        ui->passLabel3->hide();
        ui->passEdit3->hide();
        title = tr("Unlock wallet\nfor staking");
        initWatch(ui->layoutEdit);
        break;
    case Mode::Unlock: // Ask passphrase
        ui->warningLabel->setText(tr("This operation needs your wallet passphrase to unlock the wallet."));
        ui->passLabel2->hide();
        ui->passEdit2->hide();
        ui->layoutEdit2->hide();
        ui->passLabel3->hide();
        ui->passEdit3->hide();
        title = tr("Unlock wallet");
        initWatch(ui->layoutEdit);
        break;
    case Mode::Decrypt: // Ask passphrase
        ui->warningLabel->setText(tr("This operation needs your wallet passphrase to decrypt the wallet."));
        ui->passLabel2->hide();
        ui->passEdit2->hide();
        ui->layoutEdit2->hide();
        ui->passLabel3->hide();
        ui->passEdit3->hide();
        title = tr("Decrypt wallet");
        initWatch(ui->layoutEdit);
        break;
    case Mode::ChangePass: // Ask old passphrase + new passphrase x2
        title = tr("Change passphrase");
        ui->warningLabel->setText(tr("Enter the old and new passphrase to the wallet."));
        initWatch(ui->layoutEdit);
        break;
    }

    ui->labelTitle->setText(title);

    textChanged();
    connect(btnWatch, &QCheckBox::clicked, this, &AskPassphraseDialog::onWatchClicked);
    connect(ui->passEdit1, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
    connect(ui->passEdit2, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
    connect(ui->passEdit3, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
    connect(ui->pushButtonOk, &QPushButton::clicked, this, &AskPassphraseDialog::accept);
    connect(ui->btnEsc, &QPushButton::clicked, this, &AskPassphraseDialog::close);
}

void AskPassphraseDialog::onWatchClicked()
{
    int state = btnWatch->checkState();
    ui->passEdit3->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password );
    ui->passEdit2->setEchoMode(state== Qt::Checked ? QLineEdit::Normal : QLineEdit::Password );
    ui->passEdit1->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password );
}

AskPassphraseDialog::~AskPassphraseDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->passEdit1->setText(QString(" ").repeated(ui->passEdit1->text().size()));
    ui->passEdit2->setText(QString(" ").repeated(ui->passEdit2->text().size()));
    ui->passEdit3->setText(QString(" ").repeated(ui->passEdit3->text().size()));
    delete ui;
}

void AskPassphraseDialog::showEvent(QShowEvent *event)
{
    if (mode == Mode::Encrypt && ui->passEdit2) ui->passEdit2->setFocus();
    else if (ui->passEdit1) ui->passEdit1->setFocus();
}

void AskPassphraseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if (!model)
        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passEdit1->text().toStdString().c_str());
    newpass1.assign(ui->passEdit2->text().toStdString().c_str());
    newpass2.assign(ui->passEdit3->text().toStdString().c_str());

    switch (mode) {
    case Mode::Encrypt: {
        if (newpass1.empty() || newpass2.empty()) {
            // Cannot encrypt with empty passphrase
            break;
        }
        hide();
        bool ret = openStandardDialog(
                tr("Confirm wallet encryption"),
                "<b>" + tr("WARNING") + ":</b> " + tr("If you encrypt your wallet and lose your passphrase, you will") +
                " <b>" + tr("LOSE ALL OF YOUR COINS") + "</b>!<br><br>" + tr("Are you sure you wish to encrypt your wallet?"),
                tr("ENCRYPT"), tr("CANCEL")
        );
        if (ret) {
            newpassCache = newpass1;
            hemisGUI* window = static_cast<hemisGUI*>(parentWidget());
            LoadingDialog *dialog = new LoadingDialog(window);
            dialog->execute(this, 1);
            openDialogWithOpaqueBackgroundFullScreen(dialog, window);
        } else {
            QDialog::reject(); // Cancelled
        }
    } break;
    case Mode::UnlockAnonymize:
        if (!model->setWalletLocked(false, oldpass, true)) {
            QMessageBox::critical(this, tr("Wallet unlock failed"),
                                  tr("The passphrase entered for the wallet decryption was incorrect."));
        } else {
            QDialog::accept(); // Success
        }
        break;
    case Mode::Unlock:
        if (!model->setWalletLocked(false, oldpass, false)) {
            QMessageBox::critical(this, tr("Wallet unlock failed"),
                tr("The passphrase entered for the wallet decryption was incorrect."));
        } else {
            QDialog::accept(); // Success
        }
        break;
    case Mode::Decrypt:
        if (!model->setWalletEncrypted(false, oldpass)) {
            QMessageBox::critical(this, tr("Wallet decryption failed"),
                tr("The passphrase entered for the wallet decryption was incorrect."));
        } else {
            QDialog::accept(); // Success
        }
        break;
    case Mode::ChangePass:
        if (newpass1 == newpass2) {
            if (model->changePassphrase(oldpass, newpass1)) {
                hide();
                openStandardDialog(tr("Wallet encrypted"),tr("Wallet passphrase was successfully changed."));
                QDialog::accept(); // Success
            } else {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                    tr("The passphrase entered for the wallet decryption was incorrect."));
            }
        } else {
            QMessageBox::critical(this, tr("Wallet encryption failed"),
                tr("The supplied passphrases do not match."));
        }
        break;
    }
}

void AskPassphraseDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch (mode) {
    case Mode::Encrypt: // New passphrase x2
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty() && // Passphrases are not empty
                     ui->passEdit2->text() == ui->passEdit3->text();                         // Passphrases match eachother
        break;
    case Mode::UnlockAnonymize: // Old passphrase x1
    case Mode::Unlock:          // Old passphrase x1
    case Mode::Decrypt:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case Mode::ChangePass: // Old passphrase x1, new passphrase x2
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty() && // New passphrases are not empty
                     ui->passEdit2->text() == ui->passEdit3->text() &&                       // New passphrases match eachother
                     !ui->passEdit1->text().isEmpty();                                       // Old passphrase is not empty
        break;
    }
    ui->pushButtonOk->setEnabled(acceptable);
}

bool AskPassphraseDialog::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        // Detect Caps Lock key press.
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }

        updateWarningsLabel();

        // Detect Enter key press
        if ((ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) && ui->pushButtonOk->isEnabled()) {
            accept();
        }
    }
    return QDialog::event(event);
}

bool AskPassphraseDialog::eventFilter(QObject* object, QEvent* event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar* psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && *psz >= 'a' && *psz <= 'z') || (!fShift && *psz >= 'A' && *psz <= 'Z')) {
                fCapsLock = true;
            } else if (psz->isLetter()) {
                fCapsLock = false;
            }
        }
    }
    updateWarningsLabel();

    return QDialog::eventFilter(object, event);
}

bool AskPassphraseDialog::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn)
{
    hemisGUI* gui = static_cast<hemisGUI*>(parentWidget());
    DefaultDialog *confirmDialog = new DefaultDialog(gui);
    confirmDialog->setText(title, body, okBtn, cancelBtn);
    confirmDialog->adjustSize();
    openDialogWithOpaqueBackground(confirmDialog, gui);
    bool ret = confirmDialog->isOk;
    confirmDialog->deleteLater();
    return ret;
}

void AskPassphraseDialog::updateWarningsLabel()
{
    // Merge warning labels together if there's two warnings
    bool validPassphrases = false;
    validPassphrases = ui->passEdit2->text() == ui->passEdit3->text();
    QString warningStr = "";
    if (fCapsLock || !validPassphrases) warningStr += tr("WARNING:") + "<br>";
    if (fCapsLock) warningStr += "* " + tr("The caps lock key is on!");
    if (fCapsLock && !validPassphrases) warningStr += "<br>";
    if (!validPassphrases) warningStr += "* " + tr("Passphrases do not match!");

    if (warningStr.isEmpty()) {
        ui->passWarningLabel->clear();
        ui->passWarningLabel->setVisible(false);
    } else {
        ui->passWarningLabel->setText(warningStr);
        ui->passWarningLabel->setVisible(true);
    }
}

void AskPassphraseDialog::warningMessage()
{
    hide();
    static_cast<hemisGUI*>(parentWidget())->showHide(true);
    openStandardDialog(
            tr("Wallet encrypted"),
            "<qt>" +
            tr("Your wallet is now encrypted. "
               "Remember that encrypting your wallet cannot fully protect "
               "your HMSs from being stolen by malware infecting your computer.") +
            "<br><br><b>" +
            tr("IMPORTANT: Any previous backups you have made of your wallet file "
               "should be replaced with the newly generated, encrypted wallet file. "
               "For security reasons, previous backups of the unencrypted wallet file "
               "will become useless as soon as you start using the new, encrypted wallet.") +
            "</b></qt>",
            tr("OK")
            );
}

void AskPassphraseDialog::errorEncryptingWallet()
{
    QMessageBox::critical(this, tr("Wallet encryption failed"),
                          tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
}

void AskPassphraseDialog::run(int type)
{
    if (type == 1) {
        if (!newpassCache.empty()) {
            QMetaObject::invokeMethod(this, "hide", Qt::QueuedConnection);
            if (model->setWalletEncrypted(true, newpassCache)) {
                QMetaObject::invokeMethod(this, "warningMessage", Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(this, "errorEncryptingWallet", Qt::QueuedConnection);
            }
            newpassCache.clear();
            QDialog::accept(); // Success
        }
    }
}
void AskPassphraseDialog::onError(QString error, int type)
{
    newpassCache.clear();
    LogPrintf("Error encrypting wallet, %s\n", error.toStdString());
    QMetaObject::invokeMethod(this, "errorEncryptingWallet", Qt::QueuedConnection);
}

void AskPassphraseDialog::initWatch(QWidget *parent)
{
    btnWatch = new QCheckBox(parent);
    setCssProperty(btnWatch, "btn-watch-password");
    btnWatch->setChecked(false);
    QSize BUTTON_CONTACT_SIZE = QSize(24, 24);
    btnWatch->setMinimumSize(BUTTON_CONTACT_SIZE);
    btnWatch->setMaximumSize(BUTTON_CONTACT_SIZE);
    btnWatch->show();
    btnWatch->raise();

    int posYY = 8;
    btnWatch->move(450, posYY);
}
