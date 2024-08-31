// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_RECEIVEDIALOG_H
#define PIVX_QT_PIVX_RECEIVEDIALOG_H

#include "qt/pivx/focuseddialog.h"

class SendCoinsRecipient;

namespace Ui {
class ReceiveDialog;
}

class ReceiveDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit ReceiveDialog(QWidget *parent = nullptr);
    ~ReceiveDialog();

    void updateQr(const QString& address);

private Q_SLOTS:
    void onCopy();
private:
    Ui::ReceiveDialog *ui{nullptr};
    SendCoinsRecipient *info{nullptr};
};

#endif // PIVX_QT_PIVX_RECEIVEDIALOG_H
