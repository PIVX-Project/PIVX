// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNINFODIALOG_H
#define MNINFODIALOG_H

#include "qt/pivx/focuseddialog.h"
#include "qt/pivx/snackbar.h"

#include "interfaces/tiertwo.h"
#include "optional.h"

class WalletModel;

namespace Ui {
class MnInfoDialog;
}

class MnInfoDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit MnInfoDialog(QWidget *parent = nullptr);
    ~MnInfoDialog() override;

    bool exportMN = false;

    void setData(const QString& _pubKey,
                 const QString& name,
                 const QString& address,
                 const QString& _txId,
                 const QString& outputIndex,
                 const QString& status,
                 const Optional<DMNData>& dmnData);

public Q_SLOTS:
    void reject() override;

private:
    Ui::MnInfoDialog *ui;
    SnackBar *snackBar = nullptr;
    int nDisplayUnit = 0;
    WalletModel *model = nullptr;
    QString txId;
    QString pubKey;
    Optional<DMNData> dmnData{nullopt};

    void copyInform(const QString& copyStr, const QString& message);
    void setDMNDataVisible(bool show);
};

#endif // MNINFODIALOG_H
