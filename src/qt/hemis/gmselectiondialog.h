// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef GM_SELECTION_DEFAULTDIALOG_H
#define GM_SELECTION_DEFAULTDIALOG_H

#include <QDialog>
#include <map>

namespace Ui {
    class GmSelectionDialog;
}

class GMModel;
class QTreeWidgetItem;
class GmInfo;
struct VoteInfo;

class GmSelectionDialog : public QDialog
{
Q_OBJECT

public:
    explicit GmSelectionDialog(QWidget *parent);
    ~GmSelectionDialog();

    void setModel(GMModel* _gmModel, int minVoteUpdateTimeInSecs);
    void updateView();
    // Sets the GMs who already voted for this proposal
    void setGmVoters(const std::vector<VoteInfo>& _votes);
    // Return the GMs who are going to vote for this proposal
    std::vector<std::string> getSelectedGmAlias();

public Q_SLOTS:
    void viewItemChanged(QTreeWidgetItem*, int);
    void selectAll();

private:
    Ui::GmSelectionDialog *ui;
    GMModel* gmModel{nullptr};
    // Consensus param, the minimum time that need to pass
    // to be able to broadcast another vote with the same GM.
    int minVoteUpdateTimeInSecs{0};
    int colCheckBoxWidth_treeMode{50};
    // selected GMs alias
    std::vector<std::string> selectedGmList;
    // GM alias -> VoteInfo for a certain proposal
    std::map<std::string, VoteInfo> votes;

    enum {
        COLUMN_CHECKBOX,
        COLUMN_NAME,
        COLUMN_VOTE,
        COLUMN_STATUS
    };

    void appendItem(QFlags<Qt::ItemFlag> flgCheckbox,
                    QFlags<Qt::ItemFlag> flgTristate,
                    const QString& gmName,
                    const QString& gmStats,
                    VoteInfo* ptrVoteInfo);
};

#endif // GM_SELECTION_DEFAULTDIALOG_H
