// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef VOTEDIALOG_H
#define VOTEDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QProgressBar>

#include "qt/pivx/governancemodel.h"
#include <memory>

namespace Ui {
class VoteDialog;
}

struct ProposalInfo;
struct VoteInfo;
class GMModel;
class GmSelectionDialog;
class GovernanceModel;
class SnackBar;

class VoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VoteDialog(QWidget *parent, GovernanceModel* _govModel, GMModel* _gmModel);
    ~VoteDialog();

    void showEvent(QShowEvent *event) override;
    void setProposal(const ProposalInfo& prop);

public Q_SLOTS:
    void onAcceptClicked();
    void onCheckBoxClicked(QCheckBox* checkBox, QProgressBar* progressBar, bool isVoteYes);
    void onGmSelectionClicked();

private:
    Ui::VoteDialog *ui;
    GovernanceModel* govModel{nullptr};
    GMModel* gmModel{nullptr};
    SnackBar* snackBar{nullptr};

    QCheckBox* checkBoxNo{nullptr};
    QCheckBox* checkBoxYes{nullptr};
    QProgressBar* progressBarNo{nullptr};
    QProgressBar* progressBarYes{nullptr};

    std::unique_ptr<ProposalInfo> proposal;
    std::vector<VoteInfo> votes;
    GmSelectionDialog* gmSelectionDialog{nullptr};
    std::vector<std::string> vecSelectedGm;

    void initVoteCheck(QWidget* container, QCheckBox* checkBox, QProgressBar* progressBar,
                       const QString& text, Qt::LayoutDirection direction, bool isVoteYes);

    void inform(const QString& text);
    void updateGmSelectionNum();
};

#endif // VOTEDIALOG_H
