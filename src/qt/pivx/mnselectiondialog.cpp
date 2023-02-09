// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/mnselectiondialog.h"
#include "qt/pivx/forms/ui_mnselectiondialog.h"
#include "qt/pivx/mnmodel.h"
#include "qt/pivx/qtutils.h"

MnSelectionDialog::MnSelectionDialog(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::MnSelectionDialog)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssProperty(ui->labelMessage, "text-main-grey");
    setCssProperty(ui->btnEsc, "ic-chevron-left");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    setCssProperty(ui->btnSave, "btn-primary");
    setCssProperty(ui->containerAmountOfVotes, "container-border-light");
    setCssProperty(ui->labelAmountOfVotesText, "text-body-dialog");
    setCssProperty(ui->labelAmountOfVotes, "text-body-dialog");
    setCssProperty(ui->btnSelectAll, "btn-dialog-secondary");

    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, colCheckBoxWidth_treeMode);
    ui->treeWidget->setColumnWidth(COLUMN_NAME, 110);
    ui->treeWidget->setColumnWidth(COLUMN_VOTE, 50);
    ui->treeWidget->setColumnWidth(COLUMN_STATUS, 60);
    ui->treeWidget->header()->setStretchLastSection(true);
    ui->treeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->treeWidget->setRootIsDecorated(false);
    ui->treeWidget->setFocusPolicy(Qt::NoFocus);
    ui->treeWidget->model()->setHeaderData(COLUMN_NAME, Qt::Horizontal, Qt::AlignLeft , Qt::TextAlignmentRole);
    ui->treeWidget->model()->setHeaderData(COLUMN_VOTE, Qt::Horizontal, Qt::AlignHCenter , Qt::TextAlignmentRole);
    ui->treeWidget->model()->setHeaderData(COLUMN_STATUS, Qt::Horizontal, Qt::AlignHCenter , Qt::TextAlignmentRole);

    connect(ui->btnEsc, &QPushButton::clicked, this, &MnSelectionDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &MnSelectionDialog::close);
    connect(ui->btnSave, &QPushButton::clicked, this, &MnSelectionDialog::accept);
    connect(ui->btnSelectAll, &QPushButton::clicked, this, &MnSelectionDialog::selectAll);
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &MnSelectionDialog::viewItemChanged);
}

void MnSelectionDialog::setModel(MNModel* _mnModel, int _minVoteUpdateTimeInSecs)
{
    mnModel = _mnModel;
    minVoteUpdateTimeInSecs = _minVoteUpdateTimeInSecs;
}

void MnSelectionDialog::setMnVoters(const std::vector<VoteInfo>& _votes)
{
    for (const auto& vote : _votes) {
        votes.emplace(vote.mnAlias, vote);
    }
}

class MnInfo {
public:
    explicit MnInfo(const QString& _alias,
                    const QString& _status) : alias(_alias), status(_status) {}
    ~MnInfo() {}

    QString alias;
    QString status;
};

void MnSelectionDialog::viewItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == COLUMN_CHECKBOX) {
        MnInfo mnInfo(item->text(COLUMN_NAME), item->text(COLUMN_STATUS));
        if (mnInfo.alias.isEmpty()) return;
        auto it = std::find(selectedMnList.begin(), selectedMnList.end(), mnInfo.alias.toStdString());
        if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked) {
            if (it != selectedMnList.end()) {
                selectedMnList.erase(it);
                ui->labelAmountOfVotes->setText(QString::number((int)selectedMnList.size()));
            }
        } else if (item->isDisabled()) {
            item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        } else {
            if (it == selectedMnList.end()) {
                selectedMnList.emplace_back(mnInfo.alias.toStdString());
                ui->labelAmountOfVotes->setText(QString::number((int)selectedMnList.size()));
            }
        }
    }
}

void MnSelectionDialog::selectAll()
{
    const bool fSelectAll = ui->btnSelectAll->isChecked();
    Qt::CheckState wantedState = fSelectAll ? Qt::Checked : Qt::Unchecked;
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != wantedState)
            ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, wantedState);
    ui->treeWidget->setEnabled(true);
    if (!fSelectAll) {
        selectedMnList.clear();
    }
    updateView();
    ui->btnSelectAll->setText(fSelectAll ? tr("Unselect All") : tr("Select All"));
}

void MnSelectionDialog::updateView()
{
    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise the labels update would be called for every checked checkbox
    QFlags<Qt::ItemFlag> flgCheckbox = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    bool isLegacyObselete = mnModel->isLegacySystemObsolete();
    for (int i = 0; i < mnModel->rowCount(); ++i) {
        uint8_t mnType = mnModel->index(i, MNModel::TYPE, QModelIndex()).data().toUInt();
        bool acceptLegacy = mnType == MNViewType::LEGACY && !isLegacyObselete;
        if (acceptLegacy || mnType & MNViewType::DMN_VOTER) {
            QString alias = mnModel->index(i, MNModel::ALIAS, QModelIndex()).data().toString();
            QString status = mnModel->index(i, MNModel::STATUS, QModelIndex()).data().toString();
            VoteInfo* ptrVoteInfo{nullptr};
            auto it = votes.find(alias.toStdString());
            if (it != votes.end()) { ptrVoteInfo = &it->second; }
            appendItem(flgCheckbox, flgTristate, alias, status, ptrVoteInfo);
        }
    }

    // save COLUMN_CHECKBOX width for tree-mode
    colCheckBoxWidth_treeMode = std::max(110, ui->treeWidget->columnWidth(COLUMN_CHECKBOX));
    // minimize COLUMN_CHECKBOX width in list-mode (need to display only the check box)
    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 40);

    ui->treeWidget->setEnabled(true);
}

void MnSelectionDialog::appendItem(QFlags<Qt::ItemFlag> flgCheckbox,
                                   QFlags<Qt::ItemFlag> flgTristate,
                                   const QString& mnName,
                                   const QString& mnStatus,
                                   VoteInfo* ptrVoteInfo)
{
    QTreeWidgetItem* itemOutput = new QTreeWidgetItem(ui->treeWidget);
    itemOutput->setFlags(flgCheckbox);
    itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
    itemOutput->setText(COLUMN_NAME, mnName);
    itemOutput->setText(COLUMN_STATUS, mnStatus);
    itemOutput->setToolTip(COLUMN_STATUS, "Masternode status"); // future: add status description
    itemOutput->setTextAlignment(COLUMN_STATUS, Qt::AlignHCenter);
    itemOutput->setTextAlignment(COLUMN_VOTE, Qt::AlignHCenter);
    if (ptrVoteInfo) {
        itemOutput->setText(COLUMN_VOTE, ptrVoteInfo->vote == VoteInfo::YES ? tr("Yes") : tr("No"));
        itemOutput->setToolTip(COLUMN_VOTE, tr("The direction of the already broadcasted vote"));
    } else {
        itemOutput->setText(COLUMN_VOTE, "-");
        itemOutput->setToolTip(COLUMN_VOTE, tr("No vote has been emitted from this Masternode"));
    }

    if (std::find(selectedMnList.begin(), selectedMnList.end(), mnName.toStdString()) != selectedMnList.end()) {
        itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
    } else {
        itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
    }

    if (mnStatus != "ENABLED") {
        itemOutput->setDisabled(true);
    }

    // Disable MNs that will not be able to vote for the proposal until the minimum vote time passes.
    if (ptrVoteInfo && ptrVoteInfo->time + minVoteUpdateTimeInSecs > GetAdjustedTime()) {
        itemOutput->setDisabled(true);
        QString disabledTooltip{tr("Time between votes is too soon, have to wait %1 minutes to change your vote").arg(minVoteUpdateTimeInSecs/60)};
        itemOutput->setToolTip(COLUMN_CHECKBOX, disabledTooltip);
        itemOutput->setToolTip(COLUMN_NAME, disabledTooltip);
    }
}

std::vector<std::string> MnSelectionDialog::getSelectedMnAlias()
{
    return selectedMnList;
}

MnSelectionDialog::~MnSelectionDialog()
{
    delete ui;
}
