// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The PHORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proposallist.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "proposalfilterproxy.h"
#include "proposalrecord.h"
#include "proposaltablemodel.h"
#include "activemasternode.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpc/server.h"
#include "utilmoneystr.h"
#include "util.h"
#include "obfuscation.h"

#include "ui_interface.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include <QSettings>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

ProposalList::ProposalList(QWidget *parent) :
    QWidget(parent),
    proposalTableModel(0),
    proposalProxyModel(0),
    proposalList(0),
    columnResizingFixer(0)
{
    proposalTableModel = new ProposalTableModel(this);
    QSettings settings;

    setContentsMargins(0,0,0,0);

    hlayout = new ColumnAlignedLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->setSpacing(0);

    proposalWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    proposalWidget->setPlaceholderText(tr("Enter proposal name"));
#endif
    proposalWidget->setObjectName("proposalWidget");
    hlayout->addWidget(proposalWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    amountWidget->setPlaceholderText(tr("Min amount"));
#endif
    amountWidget->setValidator(new QDoubleValidator(0, 1e20, 0, this));
    amountWidget->setObjectName("amountWidget");
    hlayout->addWidget(amountWidget);

    startDateWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    startDateWidget->setPlaceholderText(tr("Start Block"));
#endif
    startDateWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    startDateWidget->setObjectName("startDateWidget");
    hlayout->addWidget(startDateWidget);

    endDateWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    endDateWidget->setPlaceholderText(tr("End Block"));
#endif
    endDateWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    endDateWidget->setObjectName("endDateWidget");
    hlayout->addWidget(endDateWidget);

    yesVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    yesVotesWidget->setPlaceholderText(tr("Min yes votes"));
#endif
    yesVotesWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    yesVotesWidget->setObjectName("yesVotesWidget");
    hlayout->addWidget(yesVotesWidget);

    noVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    noVotesWidget->setPlaceholderText(tr("Min no votes"));
#endif
    noVotesWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    noVotesWidget->setObjectName("noVotesWidget");
    hlayout->addWidget(noVotesWidget);

    abstainVotesWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    abstainVotesWidget->setPlaceholderText(tr("Min abstain votes"));
#endif
    abstainVotesWidget->setValidator(new QIntValidator(0, INT_MAX, this));
    abstainVotesWidget->setObjectName("abstainVotesWidget");
    hlayout->addWidget(abstainVotesWidget);

    percentageWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    percentageWidget->setPlaceholderText(tr("Min percentage"));
#endif
    percentageWidget->setValidator(new QIntValidator(-100, 100, this));
    percentageWidget->setObjectName("percentageWidget");
    hlayout->addWidget(percentageWidget);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setSpacing(0);

    QTableView *view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(view);
    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    hlayout->addSpacing(width);
    hlayout->setTableColumnsToTrack(view->horizontalHeader());

    connect(view->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), SLOT(invalidateAlignedLayout()));
    connect(view->horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(invalidateAlignedLayout()));

    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    proposalList = view;

    QHBoxLayout *actionBar = new QHBoxLayout();
    actionBar->setSpacing(11);
    actionBar->setContentsMargins(0,20,0,20);

    QPushButton *voteYesButton = new QPushButton(tr("Vote Yes"), this);
    voteYesButton->setToolTip(tr("Yote Yes on the selected proposal"));
    actionBar->addWidget(voteYesButton);

    QPushButton *voteAbstainButton = new QPushButton(tr("Vote Abstain"), this);
    voteAbstainButton->setToolTip(tr("Yote Abstain on the selected proposal"));
    actionBar->addWidget(voteAbstainButton);

    QPushButton *voteNoButton = new QPushButton(tr("Vote No"), this);
    voteNoButton->setToolTip(tr("Yote No on the selected proposal"));
    actionBar->addWidget(voteNoButton);

    secondsLabel = new QLabel();
    actionBar->addWidget(secondsLabel);
    actionBar->addStretch();

    vlayout->addLayout(actionBar);

    QAction *voteYesAction = new QAction(tr("Vote yes"), this);
    QAction *voteAbstainAction = new QAction(tr("Vote abstain"), this);
    QAction *voteNoAction = new QAction(tr("Vote no"), this);
    QAction *copyUrlAction = new QAction(tr("Copy proposal URL"), this);

    contextMenu = new QMenu(this);
    contextMenu->addAction(voteYesAction);
    contextMenu->addAction(voteAbstainAction);
    contextMenu->addAction(voteNoAction);
    contextMenu->addSeparator();
    contextMenu->addAction(copyUrlAction);

    connect(voteYesButton, SIGNAL(clicked()), this, SLOT(voteYes()));
    connect(voteAbstainButton, SIGNAL(clicked()), this, SLOT(voteAbstain()));
    connect(voteNoButton, SIGNAL(clicked()), this, SLOT(voteNo()));

    connect(proposalWidget, SIGNAL(textChanged(QString)), this, SLOT(changedProposal(QString)));
    connect(startDateWidget, SIGNAL(textChanged(QString)), this, SLOT(chooseStartDate(QString)));
    connect(endDateWidget, SIGNAL(textChanged(QString)), this, SLOT(chooseEndDate(QString)));
    connect(yesVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedYesVotes(QString)));
    connect(noVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedNoVotes(QString)));
    connect(abstainVotesWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAbstainVotes(QString)));
    connect(amountWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAmount(QString)));
    connect(percentageWidget, SIGNAL(textChanged(QString)), this, SLOT(changedPercentage(QString)));

    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    connect(voteYesAction, SIGNAL(triggered()), this, SLOT(voteYes()));
    connect(voteNoAction, SIGNAL(triggered()), this, SLOT(voteNo()));
    connect(voteAbstainAction, SIGNAL(triggered()), this, SLOT(voteAbstain()));

    connect(copyUrlAction, SIGNAL(triggered()), this, SLOT(copyProposalUrl()));

    proposalProxyModel = new ProposalFilterProxy(this);
    proposalProxyModel->setSourceModel(proposalTableModel);
    proposalProxyModel->setDynamicSortFilter(true);
    proposalProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proposalProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    proposalProxyModel->setSortRole(Qt::EditRole);

    proposalList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    proposalList->setModel(proposalProxyModel);
    proposalList->setAlternatingRowColors(true);
    proposalList->setSelectionBehavior(QAbstractItemView::SelectRows);
    proposalList->setSortingEnabled(true);
    proposalList->sortByColumn(ProposalTableModel::YesVotes, Qt::DescendingOrder);
    proposalList->verticalHeader()->hide();

    proposalList->setColumnWidth(ProposalTableModel::Proposal, PROPOSAL_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::Amount, AMOUNT_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::StartDate, START_DATE_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::EndDate, END_DATE_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::YesVotes, YES_VOTES_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::NoVotes, NO_VOTES_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::AbstainVotes, ABSTAIN_COLUMN_WIDTH);
    proposalList->setColumnWidth(ProposalTableModel::Percentage, PERCENTAGE_COLUMN_WIDTH);

    connect(proposalList->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(computeSum()));

    columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(proposalList, PROPOSAL_COLUMN_WIDTH, MINIMUM_COLUMN_WIDTH);

    nLastUpdate = GetTime();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(refreshProposals()));
    timer->start(1000);

    setLayout(vlayout);
}

void ProposalList::invalidateAlignedLayout() {
    hlayout->invalidate();
}

void ProposalList::refreshProposals(bool force) {
    int64_t secondsRemaining = nLastUpdate - GetTime() + PROPOSALLIST_UPDATE_SECONDS;

    QString secOrMinutes = (secondsRemaining / 60 > 1) ? tr("minute(s)") : tr("second(s)");
    secondsLabel->setText(tr("List will be updated in %1 %2").arg((secondsRemaining > 60) ? QString::number(secondsRemaining / 60) : QString::number(secondsRemaining), secOrMinutes));

    if(secondsRemaining > 0 && !force) return;
    nLastUpdate = GetTime();

    proposalTableModel->refreshProposals();

    secondsLabel->setText(tr("List will be updated in 0 second(s)"));
}

void ProposalList::changedAmount(const QString &minAmount)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinAmount(minAmount.toInt());
}

void ProposalList::changedPercentage(const QString &minPercentage)
{
    if(!proposalProxyModel)
        return;

    int value = minPercentage == "" ? -100 : minPercentage.toInt();

    proposalProxyModel->setMinPercentage(value);
}

void ProposalList::changedProposal(const QString &proposal)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setProposal(proposal);
}

void ProposalList::changedYesVotes(const QString &minYesVotes)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinYesVotes(minYesVotes.toInt());
}

void ProposalList::chooseStartDate(const QString &startDate)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinYesVotes(startDate.toInt());
}

void ProposalList::chooseEndDate(const QString &endDate)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinYesVotes(endDate.toInt());
}

void ProposalList::changedNoVotes(const QString &minNoVotes)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinNoVotes(minNoVotes.toInt());
}

void ProposalList::changedAbstainVotes(const QString &minAbstainVotes)
{
    if(!proposalProxyModel)
        return;

    proposalProxyModel->setMinAbstainVotes(minAbstainVotes.toInt());
}

void ProposalList::contextualMenu(const QPoint &point)
{
    QModelIndex index = proposalList->indexAt(point);
    QModelIndexList selection = proposalList->selectionModel()->selectedRows(0);
    if (selection.empty())
        return;

    if(index.isValid())
        contextMenu->exec(QCursor::pos());
}

void ProposalList::voteYes()
{
    vote_click_handler("yes");
}

void ProposalList::voteNo()
{
    vote_click_handler("no");
}

void ProposalList::voteAbstain()
{
    vote_click_handler("abstain");
}

void ProposalList::vote_click_handler(const std::string voteString)
{
    if(!proposalList->selectionModel())
        return;

    QModelIndexList selection = proposalList->selectionModel()->selectedRows();
    if(selection.empty())
        return;

    QString proposalName = selection.at(0).data(ProposalTableModel::ProposalRole).toString();

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm vote"),
        tr("Are you sure you want to vote <strong>%1</strong> on the proposal <strong>%2</strong>?").arg(QString::fromStdString(voteString), proposalName),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    uint256 hash;
    hash.SetHex(selection.at(0).data(ProposalTableModel::ProposalHashRole).toString().toStdString());

    int success = 0;
    int failed = 0;

    std::string strVote = voteString;
    int nVote = VOTE_ABSTAIN;
    if (strVote == "yes") nVote = VOTE_YES;
    if (strVote == "no") nVote = VOTE_NO;

    for (const auto& mne : masternodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            UniValue statusObj(UniValue::VOBJ);

            if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)) {
                failed++;
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if (pmn == NULL) {
                failed++;
                continue;
            }

            CBudgetVote vote(pmn->vin, hash, nVote);
            if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
                failed++;
                continue;
            }

            std::string strError = "";
            if (budget.UpdateProposal(vote, NULL, strError)) {
                budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
            } else {
                failed++;
            }
    }

    QMessageBox::information(this, tr("Voting"),
        tr("You voted %1 %2 time(s) successfully and failed %3 time(s) on %4").arg(QString::fromStdString(voteString), QString::number(success), QString::number(failed), proposalName));

    refreshProposals(true);
}

void ProposalList::copyProposalUrl()
{
    if(!proposalList || !proposalList->selectionModel())
        return;

    GUIUtil::copyEntryData(proposalList, 0, ProposalTableModel::ProposalUrlRole);
    QMessageBox Msgbox;
    Msgbox.setText(tr("The proposal URL has been copied to the clipboard. You can use it to visit the proposal website.\nHowever, be very careful when doing so. Some websites can try to extract your wallet information in the background or to lure you into a scam. Prefer using different devices to hold funds and to visit proposals URLs."));
    Msgbox.exec();
}

void ProposalList::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(ProposalTableModel::Proposal);
}
