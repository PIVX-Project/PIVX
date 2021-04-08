// Copyright (c) 2011-2013 The Bitcoin developers
//Copyright (c) 2017-2019 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OPTIONSDIALOG_H
#define BITCOIN_QT_OPTIONSDIALOG_H

#include <QDialog>

class OptionsModel;
class QValidatedLineEdit;
class QLineEdit;

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui
{
class OptionsDialog;
}

/** Preferences dialog. */
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent, bool enableWallet);
    ~OptionsDialog();

    void setModel(OptionsModel* model);
    void setMapper();
    void setCurrentIndex(int index);

protected:
    bool eventFilter(QObject* object, QEvent* event);

private Q_SLOTS:
    /* enable OK button */
    void enableOkButton();
    /* disable OK button */
    void disableOkButton();
    /* set OK button state (enabled / disabled) */
    void setOkButtonState(bool fState);
    void on_resetButton_clicked();
    void on_okButton_clicked();
    void on_cancelButton_clicked();

    void updateHideOrphans(bool fHide);

    void showRestartWarning(bool fPersistent = false);
    void clearStatusLabel();
    void doProxyIpChecks(QValidatedLineEdit* pUiProxyIp, QLineEdit* pUiProxyPort);

Q_SIGNALS:
    void proxyIpChecks(QValidatedLineEdit* pUiProxyIp, QLineEdit* pUiProxyPort);

private:
    Ui::OptionsDialog* ui;
    OptionsModel* model;
    QDataWidgetMapper* mapper;
    bool fProxyIpValid;
};

#endif // BITCOIN_QT_OPTIONSDIALOG_H
