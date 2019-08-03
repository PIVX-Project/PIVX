// Copyright (c) 2018-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proposalframe.h"

#include "masternode-budget.h"
#include "masternodeconfig.h"
#include "utilmoneystr.h"

#include <QLayout>
#include <QToolButton>
#include <QPushButton>
#include <QMessageBox>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QUrl>
#include <QSpacerItem>
#include <QMouseEvent>

ProposalFrame::ProposalFrame(QWidget* parent) : QFrame(parent)
{
    extended = false;
}

ProposalFrame::~ProposalFrame()
{
    if (proposalItem)
        delete proposalItem;
}

void ProposalFrame::setProposal(CBudgetProposal* proposal)
{
    this->proposal = proposal;

    proposalItem = new QVBoxLayout(this);
    proposalItem->setSpacing(0);
    proposalItem->setObjectName(QStringLiteral("proposalItem"));

    QHBoxLayout* proposalInfo = new QHBoxLayout();
    proposalInfo->setSpacing(0);
    proposalInfo->setObjectName(QStringLiteral("proposalInfo"));

    QLabel* strProposalHash = new QLabel();
    strProposalHash->setObjectName(QStringLiteral("strProposalHash"));
    strProposalHash->setText(QString::fromStdString(proposal->GetHash().ToString()));
    strProposalHash->setVisible(false);
    QLabel* strProposalName = new QLabel();
    strProposalName->setObjectName(QStringLiteral("strProposalName"));
    strProposalName->setText(QString::fromStdString(proposal->GetName()));
    QLabel* strMonthlyPayout = new QLabel();
    strMonthlyPayout->setObjectName(QStringLiteral("strMonthlyPayout"));
    strMonthlyPayout->setText(QString::fromStdString(FormatMoney(proposal->GetAmount())));
    strMonthlyPayout->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    proposalInfo->addWidget(strProposalName);
    proposalInfo->addWidget(strProposalHash);
    proposalInfo->addStretch();
    proposalInfo->addWidget(strMonthlyPayout);
    proposalItem->addLayout(proposalInfo);
}

void ProposalFrame::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
}

void ProposalFrame::setGovernancePage(GovernancePage* governancePage)
{
    this->governancePage = governancePage;
}

void ProposalFrame::mousePressEvent(QMouseEvent* event)
{

}

void ProposalFrame::refresh()
{
    if (!proposal) //TODO implement an error
        return;

    if (extended)
    {
        // URL
        proposalURL = new QHBoxLayout();

        QLabel* strProposalURL = new QLabel();
        strProposalURL->setObjectName(QStringLiteral("strProposalURL"));
        QString strURL = QString::fromStdString(proposal->GetURL());
        QString strClick = tr("Open proposal page in browser");
        strProposalURL->setText("<a href=\"" + strURL + "\">" + strClick + "</a>");
        strProposalURL->setTextFormat(Qt::RichText);
        strProposalURL->setTextInteractionFlags(Qt::TextBrowserInteraction);
        strProposalURL->setOpenExternalLinks(false);
        connect(strProposalURL, &QLabel::linkActivated, this, &ProposalFrame::proposalLink_clicked);
        proposalURL->addWidget(strProposalURL);

        QSpacerItem* spacer = new QSpacerItem(1,1, QSizePolicy::Expanding, QSizePolicy::Fixed);
        proposalURL->addSpacerItem(spacer);

        proposalItem->addLayout(proposalURL);

        //CYCLES LEFT
        strRemainingPaymentCount = new QLabel();
        strRemainingPaymentCount->setObjectName(QStringLiteral("strRemainingPaymentCount"));
        QString strPaymentCount = QString::number(proposal->GetRemainingPaymentCount()) + tr(" remaining payment(s).");
        strRemainingPaymentCount->setText(strPaymentCount);
        proposalItem->addWidget(strRemainingPaymentCount);

        //VOTES
        int nCountMyMasternodes = masternodeConfig.getCount();
        proposalVotes = new QHBoxLayout();

        QToolButton* yesButton = new QToolButton();
        yesButton->setIcon(QIcon(":/icons/yesvote"));
        if (nCountMyMasternodes < 0)
            yesButton->setEnabled(false);
        connect(yesButton, &QPushButton::clicked, this, [this]{ voteButton_clicked(1); });
        QLabel* labelYesVotes = new QLabel();
        labelYesVotes->setText(tr("Yes:"));
        QLabel* yesVotes = new QLabel();
        yesVotes->setText(QString::number(proposal->GetYeas()));

        QToolButton* abstainButton = new QToolButton();
        abstainButton->setIcon(QIcon(":/icons/abstainvote"));
        if (nCountMyMasternodes < 0)
            abstainButton->setEnabled(false);
        connect(abstainButton, &QPushButton::clicked, this, [this]{ voteButton_clicked(0); });
        QLabel* labelAbstainVotes = new QLabel();
        labelAbstainVotes->setText(tr("Abstain:"));
        QLabel* abstainVotes = new QLabel();
        abstainVotes->setText(QString::number(proposal->GetAbstains()));

        QToolButton* noButton = new QToolButton();
        noButton->setIcon(QIcon(":/icons/novote"));
        if (nCountMyMasternodes < 0)
            noButton->setEnabled(false);
        connect(noButton, &QPushButton::clicked, this, [this]{ voteButton_clicked(2); });
        QLabel* labelNoVotes = new QLabel();
        labelNoVotes->setText(tr("No:"));
        QLabel* noVotes = new QLabel();
        noVotes->setText(QString::number(proposal->GetNays()));

        proposalVotes->addWidget(yesButton);
        proposalVotes->addWidget(labelYesVotes);
        proposalVotes->addWidget(yesVotes);
        proposalVotes->addStretch();
        proposalVotes->addWidget(abstainButton);
        proposalVotes->addWidget(labelAbstainVotes);
        proposalVotes->addWidget(abstainVotes);
        proposalVotes->addStretch();
        proposalVotes->addWidget(labelNoVotes);
        proposalVotes->addWidget(noVotes);
        proposalVotes->addWidget(noButton);
        proposalItem->addLayout(proposalVotes);
    } else
    {
        delete strRemainingPaymentCount;
        QLayoutItem* child;
        while ((child = proposalVotes->takeAt(0)) != 0) {
            if (child->widget() != 0)
            {
                delete child->widget();
            }
            delete child;
        }
        delete proposalVotes;
        while ((child = proposalURL->takeAt(0)) != 0) {
            if (child->widget() != 0)
            {
                delete child->widget();
            }
            delete child;
        }
        delete proposalURL;
    }
}

void ProposalFrame::proposalLink_clicked(const QString &link)
{
    QMessageBox Msgbox;
    QString strMsgBox = tr("A proposal URL can be used for phishing, scams and computer viruses. Open this link only if you trust the following URL.\n");
    strMsgBox += QString::fromStdString(proposal->GetURL());
    Msgbox.setText(strMsgBox);
    Msgbox.setStandardButtons(QMessageBox::Cancel);
    QAbstractButton *openButton = Msgbox.addButton(tr("Open link"), QMessageBox::ApplyRole);
    QAbstractButton *copyButton = Msgbox.addButton(tr("Copy link"), QMessageBox::ApplyRole);

    governancePage->lockUpdating(true);
    Msgbox.exec();

    if (Msgbox.clickedButton() == openButton) {
        QDesktopServices::openUrl(QUrl(QString::fromStdString(proposal->GetURL())));
    }
    if (Msgbox.clickedButton() == copyButton) {
        QGuiApplication::clipboard()->setText(QString::fromStdString(proposal->GetURL()));
    }
    governancePage->lockUpdating(false);
}

void ProposalFrame::extend()
{
    extended = true;
    refresh();
}

void ProposalFrame::close()
{
    extended = false;
    refresh();
}

void ProposalFrame::mouseReleaseEvent(QMouseEvent* event)
{
    if (!governancePage) //TODO implement an error
        return;
    if (event->button() == Qt::RightButton)
        return;
    extended = !extended;
    if (extended)
        governancePage->setExtendedProposal(proposal);
    else
        governancePage->setExtendedProposal(nullptr);
}

void ProposalFrame::voteButton_clicked(int nVote)
{
    if (!walletModel) return;

    // Request unlock if wallet was locked or unlocked for mixing:
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::UI_Vote, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            governancePage->lockUpdating(true);
            QMessageBox::warning(this, tr("Wallet Locked"), tr("You must unlock your wallet to vote."), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
    }

    // Display message box
    QString questionString = tr("Do you want to vote %1 on").arg(nVote == 1 ? "yes" : (nVote == 2 ? "no" : "abstain")) + " " + QString::fromStdString(proposal->GetName()) + " ";
    questionString += tr("using all your masternodes?");
    questionString += "<br /><br />";
    questionString += tr("Proposal Hash:") + " " + QString::fromStdString(proposal->GetHash().ToString()) + "<br />";
    questionString += tr("Proposal URL:") + " " + QString::fromStdString(proposal->GetURL());
    governancePage->lockUpdating(true);
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm Vote"),
        questionString,
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        return;
    }

    governancePage->lockUpdating(false);
    SendVote(proposal->GetHash().ToString(), nVote);
}

void ProposalFrame::SendVote(std::string strHash, int nVote)
{
    uint256 hash = uint256(strHash);
    int failed = 0, success = 0;
    std::string mnresult;
    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        std::string errorMessage;
        std::vector<unsigned char> vchMasterNodeSignature;
        std::string strMasterNodeSignMessage;

        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;
        CPubKey pubKeyMasternode;
        CKey keyMasternode;

        if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)) {
            mnresult += mne.getAlias() + ": " + "Masternode signing error, could not set key correctly: " + errorMessage + "<br />";
            failed++;
            continue;
        }

        CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
        if (pmn == NULL) {
            mnresult += mne.getAlias() + ": " + "Can't find masternode by pubkey" + "<br />";
            failed++;
            continue;
        }

        CBudgetVote vote(pmn->vin, hash, nVote);
        if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
            mnresult += mne.getAlias() + ": " + "Failure to sign" + "<br />";
            failed++;
            continue;
        }

        std::string strError = "";
        if (budget.UpdateProposal(vote, NULL, strError)) {
            budget.mapSeenMasternodeBudgetVotes.insert(std::make_pair(vote.GetHash(), vote));
            vote.Relay();
            mnresult += mne.getAlias() + ": " + "Success!" + "<br />";
            success++;
        } else {
            mnresult += mne.getAlias() + ": " + "Failed!" + "<br />";
            failed++;
        }

    }

    QString strMessage = QString("Successfully voted with %1 masternode(s), failed with %2").arg(success).arg(failed);
    strMessage += "<br /><br /><hr />";
    strMessage += QString::fromStdString(mnresult);
    QMessageBox::information(governancePage, tr("Vote Results"), strMessage, QMessageBox::Ok, QMessageBox::Ok);
}
