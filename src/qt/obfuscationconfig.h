// Copyright (c) 2014-2016 The Dash Developers
// Copyright (c) 2016-2017 The PIVX developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OBFUSCATIONCONFIG_H
#define OBFUSCATIONCONFIG_H

#include <QDialog>

namespace Ui
{
class ObfuscationConfig;
}
class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class ObfuscationConfig : public QDialog
{
    Q_OBJECT

public:
    ObfuscationConfig(QWidget* parent = 0);
    ~ObfuscationConfig();

    void setModel(WalletModel* model);


private:
    Ui::ObfuscationConfig* ui;
    WalletModel* model;
    void configure(bool enabled, int coins, int rounds);

private slots:

    void clickBasic();
    void clickHigh();
    void clickMax();
};

#endif // OBFUSCATIONCONFIG_H
