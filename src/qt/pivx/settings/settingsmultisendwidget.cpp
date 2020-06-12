// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsmultisendwidget.h"
#include "qt/pivx/settings/forms/ui_settingsmultisendwidget.h"
#include "qt/pivx/settings/settingsmultisenddialog.h"
#include "qt/pivx/qtutils.h"
#include "addresstablemodel.h"
#include "base58.h"
#include "init.h"
#include "walletmodel.h"
#include "wallet/wallet.h"


#define DECORATION_SIZE 65
#define NUM_ITEMS 3

MultiSendModel::MultiSendModel(QObject *parent) : QAbstractTableModel(parent)
{
    updateList();
}

void MultiSendModel::updateList()
{
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index((int) pwalletMain->vMultiSend.size(), 5, QModelIndex()) );
}

int MultiSendModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return (int) pwalletMain->vMultiSend.size();
}

QVariant MultiSendModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case PERCENTAGE:
                return pwalletMain->vMultiSend[row].second;
            case ADDRESS: {
                return QString::fromStdString(pwalletMain->vMultiSend[row].first);
            }
        }
    }
    return QVariant();
}

QModelIndex MultiSendModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return createIndex(row, column, nullptr);
}

class MultiSendHolder : public FurListRow<QWidget*>
{
public:
    MultiSendHolder();

    explicit MultiSendHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme) {}

    QWidget* createHolder(int pos) override
    {
        if (!row) {
            row = new QWidget();
            QVBoxLayout *verticalLayout_2;
            QFrame *frame_2;
            QHBoxLayout *horizontalLayout;
            QLabel *labelName;
            QSpacerItem *horizontalSpacer;
            QLabel *labelDate;
            QLabel *lblDivisory;

            if (row->objectName().isEmpty())
                row->setObjectName(QStringLiteral("multiSendrow"));
            row->resize(475, 65);
            row->setStyleSheet(QStringLiteral(""));
            setCssProperty(row, "container");
            verticalLayout_2 = new QVBoxLayout(row);
            verticalLayout_2->setSpacing(0);
            verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
            verticalLayout_2->setContentsMargins(20, 0, 20, 0);
            frame_2 = new QFrame(row);
            frame_2->setObjectName(QStringLiteral("frame_2"));
            frame_2->setStyleSheet(QStringLiteral("border:none;"));
            frame_2->setFrameShape(QFrame::StyledPanel);
            frame_2->setFrameShadow(QFrame::Raised);
            horizontalLayout = new QHBoxLayout(frame_2);
            horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
            horizontalLayout->setContentsMargins(0, -1, 0, -1);
            labelName = new QLabel(frame_2);
            labelName->setObjectName(QStringLiteral("labelAddress"));
            setCssProperty(labelName, "text-list-title1");
            horizontalLayout->addWidget(labelName);

            horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
            horizontalLayout->addItem(horizontalSpacer);

            labelDate = new QLabel(frame_2);
            labelDate->setObjectName(QStringLiteral("labelPercentage"));
            setCssProperty(labelDate, "text-list-caption-medium");
            horizontalLayout->addWidget(labelDate);

            verticalLayout_2->addWidget(frame_2);

            lblDivisory = new QLabel(row);
            lblDivisory->setObjectName(QStringLiteral("lblDivisory"));
            lblDivisory->setMinimumSize(QSize(0, 1));
            lblDivisory->setMaximumSize(QSize(16777215, 1));
            lblDivisory->setStyleSheet(QStringLiteral("background-color:#bababa;"));
            lblDivisory->setAlignment(Qt::AlignBottom | Qt::AlignLeading | Qt::AlignLeft);
            verticalLayout_2->addWidget(lblDivisory);
        }

        return row;
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override
    {
        holder->findChild<QLabel*>("labelAddress")->setText(index.data(Qt::DisplayRole).toString());
        holder->findChild<QLabel*>("labelPercentage")->setText(
                QString::number(index.sibling(index.row(), MultiSendModel::PERCENTAGE).data(Qt::DisplayRole).toInt()) + QString("%")
        );
    }

    QColor rectColor(bool isHovered, bool isSelected) override
    {
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~MultiSendHolder() override {}

    bool isLightTheme;
    QWidget *row = nullptr;
};


SettingsMultisendWidget::SettingsMultisendWidget(PWidget *parent) :
    PWidget(parent),
    ui(new Ui::SettingsMultisendWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    delegate = new FurAbstractListItemDelegate(
            DECORATION_SIZE,
            new MultiSendHolder(isLightTheme()),
            this
    );

    // Containers
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setText("Multisend");
    setCssTitleScreen(ui->labelTitle);

    ui->labelSubtitle1->setText(tr("MultiSend allows you to automatically send up to 100% of your stake or masternode reward to a list of other PIVX addresses after it matures."));
    setCssSubtitleScreen(ui->labelSubtitle1);

    //Button Group
    ui->pushLeft->setText(tr("Active"));
    setCssProperty(ui->pushLeft, "btn-check-left");
    ui->pushRight->setText(tr("Disable"));
    setCssProperty(ui->pushRight, "btn-check-right");

    setCssProperty(ui->pushImgEmpty, "img-empty-multisend");
    ui->labelEmpty->setText(tr("No active recipient yet"));
    setCssProperty(ui->labelEmpty, "text-empty");

    // CheckBox
    ui->checkBoxStake->setText(tr("Send stakes"));
    ui->checkBoxRewards->setText(tr("Send masternode rewards"));

    setCssProperty(ui->listView, "container");
    ui->listView->setItemDelegate(delegate);
    ui->listView->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listView->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listView->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Buttons
    ui->pushButtonSave->setText(tr("ADD RECIPIENT"));
    ui->pushButtonClear->setText(tr("CLEAR ALL"));
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonClear);

    connect(ui->pushButtonSave, &QPushButton::clicked, this, &SettingsMultisendWidget::onAddRecipientClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, this, &SettingsMultisendWidget::clearAll);
}

void SettingsMultisendWidget::showEvent(QShowEvent *event)
{
    if (multiSendModel) {
        multiSendModel->updateList();
        updateListState();
    }
}

void SettingsMultisendWidget::loadWalletModel()
{
    if (walletModel) {
        multiSendModel = new MultiSendModel(this);
        ui->listView->setModel(multiSendModel);
        ui->listView->setModelColumn(MultiSendModel::ADDRESS);

        ui->pushLeft->setChecked(pwalletMain->isMultiSendEnabled());
        ui->checkBoxStake->setChecked(pwalletMain->fMultiSendStake);
        ui->checkBoxRewards->setChecked(pwalletMain->fMultiSendMasternodeReward);
        connect(ui->checkBoxStake, &QCheckBox::stateChanged, this, &SettingsMultisendWidget::checkBoxChanged);
        connect(ui->checkBoxRewards, &QCheckBox::stateChanged, this, &SettingsMultisendWidget::checkBoxChanged);
        connect(ui->pushLeft, &QPushButton::clicked, this, &SettingsMultisendWidget::activate);
        connect(ui->pushRight, &QPushButton::clicked, this, &SettingsMultisendWidget::deactivate);

        updateListState();
    }
}

void SettingsMultisendWidget::updateListState()
{
    if (multiSendModel->rowCount() > 0) {
        ui->listView->setVisible(true);
        ui->emptyContainer->setVisible(false);
    } else {
        ui->listView->setVisible(false);
        ui->emptyContainer->setVisible(true);
    }
}

void SettingsMultisendWidget::clearAll()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot perform operation, wallet locked"));
        return;
    }
    std::vector<std::pair<std::string, int> > vMultiSendTemp = pwalletMain->vMultiSend;
    bool fRemoved = true;
    pwalletMain->vMultiSend.clear();
    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.EraseMultiSend(vMultiSendTemp))
        fRemoved = false;
    if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
        fRemoved = false;

    checkBoxChanged();
    multiSendModel->updateList();
    updateListState();
    inform(fRemoved ? tr("Clear succeed") : tr("Clear all failed, could not locate address in wallet file"));
}

void SettingsMultisendWidget::checkBoxChanged()
{
    pwalletMain->fMultiSendStake = ui->checkBoxStake->isChecked();
    pwalletMain->fMultiSendMasternodeReward = ui->checkBoxRewards->isChecked();
}

void SettingsMultisendWidget::onAddRecipientClicked()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot add multisend recipient, wallet locked"));
        return;
    }
    showHideOp(true);
    SettingsMultisendDialog* dialog = new SettingsMultisendDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if (dialog->isOk) {
        addMultiSend(
                dialog->getAddress(),
                dialog->getPercentage(),
                dialog->getLabel()
        );
    }
    dialog->deleteLater();
}

void SettingsMultisendWidget::addMultiSend(QString address, int percentage, QString addressLabel)
{
    std::string strAddress = address.toStdString();
    CTxDestination destAddress = DecodeDestination(strAddress);
    if (!IsValidDestination(destAddress)) {
        inform(tr("The entered address: %1 is invalid.\nPlease check the address and try again.").arg(address));
        return;
    }
    if (percentage > 100 || percentage <= 0) {
        inform(tr("Invalid percentage, please enter values from 1 to 100."));
        return;
    }

    int nMultiSendPercent = percentage;
    int nSumMultiSend = 0;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
        nSumMultiSend += pwalletMain->vMultiSend[i].second;
    if (nSumMultiSend + nMultiSendPercent > 100) {
        inform(tr("The total amount of your MultiSend vector is over 100% of your stake reward"));
        return;
    }
    std::pair<std::string, int> pMultiSend;
    pMultiSend.first = strAddress;
    pMultiSend.second = nMultiSendPercent;
    pwalletMain->vMultiSend.push_back(pMultiSend);

    if (walletModel && walletModel->getAddressTableModel()) {
        // update the address book with the label given or no label if none was given.
        std::string userInputLabel = addressLabel.toStdString();
        walletModel->updateAddressBookLabels(destAddress, (userInputLabel.empty()) ? "(no label)" : userInputLabel,
                AddressBook::AddressBookPurpose::SEND);
    }

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend)) {
        inform(tr("Error saving  MultiSend, failed saving properties to the database."));
        return;
    }

    multiSendModel->updateList();
    updateListState();
    inform("MultiSend recipient added.");
}

void SettingsMultisendWidget::activate()
{
    if (pwalletMain->isMultiSendEnabled())
        return;
    QString strRet;
    if (pwalletMain->vMultiSend.size() < 1)
        strRet = tr("Unable to activate MultiSend, no available recipients");
    else if (!(ui->checkBoxStake->isChecked() || ui->checkBoxRewards->isChecked())) {
        strRet = tr("Unable to activate MultiSend\nCheck one or both of the check boxes to send on stake and/or masternode rewards");
    } else if (IsValidDestinationString(pwalletMain->vMultiSend[0].first)) {
        pwalletMain->fMultiSendStake = ui->checkBoxStake->isChecked();
        pwalletMain->fMultiSendMasternodeReward = ui->checkBoxRewards->isChecked();

        CWalletDB walletdb(pwalletMain->strWalletFile);
        if (!walletdb.WriteMSettings(pwalletMain->fMultiSendStake, pwalletMain->fMultiSendMasternodeReward, pwalletMain->nLastMultiSendHeight))
            strRet = tr("MultiSend activated but writing settings to DB failed");
        else
            strRet = tr("MultiSend activated");
    } else
        strRet = tr("First multiSend address invalid");

    inform(strRet);
}

void SettingsMultisendWidget::deactivate()
{
    if (pwalletMain->isMultiSendEnabled()) {
        QString strRet;
        pwalletMain->setMultiSendDisabled();
        CWalletDB walletdb(pwalletMain->strWalletFile);
        inform(!walletdb.WriteMSettings(false, false, pwalletMain->nLastMultiSendHeight) ?
               tr("MultiSend deactivated but writing settings to DB failed") :
               tr("MultiSend deactivated")
        );
    }
}

void SettingsMultisendWidget::changeTheme(bool isLightTheme, QString& theme)
{
    static_cast<MultiSendHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
}

SettingsMultisendWidget::~SettingsMultisendWidget()
{
    delete ui;
}
