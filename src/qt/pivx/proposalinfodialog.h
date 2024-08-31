// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_PROPOSALINFODIALOG_H
#define PIVX_QT_PIVX_PROPOSALINFODIALOG_H

#include "qt/pivx/focuseddialog.h"
#include "qt/pivx/governancemodel.h"

struct ProposalInfo;
class SnackBar;
class WalletModel;

namespace Ui {
class ProposalInfoDialog;
}

class ProposalInfoDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit ProposalInfoDialog(QWidget *parent = nullptr);
    ~ProposalInfoDialog();
    void setProposal(const ProposalInfo& info);

public Q_SLOTS:
    void accept() override;
    void reject() override;

private:
    Ui::ProposalInfoDialog* ui;
    SnackBar* snackBar{nullptr};
    ProposalInfo info;

    void inform(const QString& msg);
};

#endif // PIVX_QT_PIVX_PROPOSALINFODIALOG_H
