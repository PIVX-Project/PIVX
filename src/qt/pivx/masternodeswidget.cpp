// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/masternodeswidget.h"
#include "qt/pivx/forms/ui_masternodeswidget.h"

#include "qt/pivx/defaultdialog.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/mnrow.h"
#include "qt/pivx/mninfodialog.h"
#include "qt/pivx/masternodewizarddialog.h"

#include "clientmodel.h"
#include "guiutil.h"
#include "interfaces/tiertwo.h"
#include "qt/pivx/mnmodel.h"
#include "qt/pivx/optionbutton.h"
#include "qt/walletmodel.h"

#define DECORATION_SIZE 65
#define NUM_ITEMS 3
#define REQUEST_START_ALL 1
#define REQUEST_START_MISSING 2

class MNHolder : public FurListRow<QWidget*>
{
public:
    explicit MNHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme) {}

    MNRow* createHolder(int pos) override
    {
        if (!cachedRow) cachedRow = new MNRow();
        return cachedRow;
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override
    {
        MNRow* row = static_cast<MNRow*>(holder);
        QString label = index.data(Qt::DisplayRole).toString();
        QString address = index.sibling(index.row(), MNModel::ADDRESS).data(Qt::DisplayRole).toString();
        QString status = index.sibling(index.row(), MNModel::STATUS).data(Qt::DisplayRole).toString();
        bool wasCollateralAccepted = index.sibling(index.row(), MNModel::WAS_COLLATERAL_ACCEPTED).data(Qt::DisplayRole).toBool();
        uint8_t type = index.sibling(index.row(), MNModel::TYPE).data(Qt::DisplayRole).toUInt();
        row->updateView("Address: " + address, label, status, wasCollateralAccepted, type);
    }

    QColor rectColor(bool isHovered, bool isSelected) override
    {
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~MNHolder() override{}

    bool isLightTheme;
    MNRow* cachedRow = nullptr;
};

MasterNodesWidget::MasterNodesWidget(PIVXGUI *parent) :
    PWidget(parent),
    ui(new Ui::MasterNodesWidget),
    isLoading(false)
{
    ui->setupUi(this);

    delegate = new FurAbstractListItemDelegate(
            DECORATION_SIZE,
            new MNHolder(isLightTheme()),
            this
    );

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    setCssProperty(ui->left, "container");
    ui->left->setContentsMargins(0,20,0,20);
    setCssProperty(ui->right, "container-right");
    ui->right->setContentsMargins(20,20,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    setCssTitleScreen(ui->labelTitle);
    ui->labelTitle->setFont(fontLight);
    setCssSubtitleScreen(ui->labelSubtitle1);

    /* Buttons */
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnPrimary(ui->pushButtonStartAll);
    setCssBtnPrimary(ui->pushButtonStartMissing);

    /* Options */
    ui->btnAbout->setTitleClassAndText("btn-title-grey", tr("What is a Masternode?"));
    ui->btnAbout->setSubTitleClassAndText("text-subtitle", tr("FAQ explaining what Masternodes are"));
    ui->btnAboutController->setTitleClassAndText("btn-title-grey", tr("What is a Controller?"));
    ui->btnAboutController->setSubTitleClassAndText("text-subtitle", tr("FAQ explaining what is a Masternode Controller"));

    setCssProperty(ui->listMn, "container");
    ui->listMn->setItemDelegate(delegate);
    ui->listMn->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listMn->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listMn->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listMn->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->emptyContainer->setVisible(false);
    setCssProperty(ui->pushImgEmpty, "img-empty-master");
    setCssProperty(ui->labelEmpty, "text-empty");

    connect(ui->pushButtonSave, &QPushButton::clicked, this, &MasterNodesWidget::onCreateMNClicked);
    connect(ui->pushButtonStartAll, &QPushButton::clicked, [this]() {
        onStartAllClicked(REQUEST_START_ALL);
    });
    connect(ui->pushButtonStartMissing, &QPushButton::clicked, [this]() {
        onStartAllClicked(REQUEST_START_MISSING);
    });
    connect(ui->listMn, &QListView::clicked, this, &MasterNodesWidget::onMNClicked);
    connect(ui->btnAbout, &OptionButton::clicked, [this](){window->openFAQ(SettingsFaqWidget::Section::MASTERNODE);});
    connect(ui->btnAboutController, &OptionButton::clicked, [this](){window->openFAQ(SettingsFaqWidget::Section::MNCONTROLLER);});
}

void MasterNodesWidget::showEvent(QShowEvent *event)
{
    if (!mnModel) return;
    const auto& updateList = [&](){
        mnModel->updateMNList();
        updateListState();
    };
    updateList();
    if (!timer) {
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, updateList);
    }
    timer->start(30000);
}

void MasterNodesWidget::hideEvent(QHideEvent *event)
{
    if (timer) timer->stop();
}

void MasterNodesWidget::setMNModel(MNModel* _mnModel)
{
    mnModel = _mnModel;
    ui->listMn->setModel(mnModel);
    ui->listMn->setModelColumn(AddressTableModel::Label);
    updateListState();
}

void MasterNodesWidget::updateListState()
{
    bool show = mnModel->rowCount() > 0;
    ui->listMn->setVisible(show);
    ui->emptyContainer->setVisible(!show);
    ui->pushButtonStartAll->setVisible(show);
}

void MasterNodesWidget::onMNClicked(const QModelIndex& _index)
{
    ui->listMn->setCurrentIndex(_index);
    QRect rect = ui->listMn->visualRect(_index);
    QPoint pos = rect.topRight();
    pos.setX((int) pos.x() - (DECORATION_SIZE * 2));
    pos.setY((int) pos.y() + (DECORATION_SIZE * 1.5));
    if (!menu) {
        menu = new TooltipMenu(window, this);
        connect(menu, &TooltipMenu::message, this, &AddressesWidget::message);
        menu->addBtn(0, tr("Start"), [this](){onEditMNClicked();});
        menu->addBtn(1, tr("Delete"), [this](){onDeleteMNClicked();});
        menu->addBtn(2, tr("Info"), [this](){onInfoMNClicked();});
        menu->adjustSize();
    } else {
        menu->hide();
    }

    index = _index;
    uint8_t mnType = index.sibling(index.row(), MNModel::TYPE).data(Qt::DisplayRole).toUInt();
    menu->showHideBtn(0, mnType == MNViewType::LEGACY);

    menu->move(pos);
    menu->show();

    // Back to regular status
    ui->listMn->scrollTo(index);
    ui->listMn->clearSelection();
    ui->listMn->setFocus();
}

bool MasterNodesWidget::checkMNsNetwork()
{
    bool isTierTwoSync = mnModel->isMNsNetworkSynced();
    if (!isTierTwoSync) inform(tr("Please wait until the node is fully synced"));
    return isTierTwoSync;
}

void MasterNodesWidget::onEditMNClicked()
{
    if (walletModel) {
        if (!walletModel->isRegTestNetwork() && !checkMNsNetwork()) return;
        uint8_t mnType = index.sibling(index.row(), MNModel::TYPE).data(Qt::DisplayRole).toUInt();
        if (mnType == MNViewType::LEGACY) {
            if (index.sibling(index.row(), MNModel::WAS_COLLATERAL_ACCEPTED).data(Qt::DisplayRole).toBool()) {
                // Start MN
                QString strAlias = this->index.data(Qt::DisplayRole).toString();
                if (ask(tr("Start Masternode"), tr("Are you sure you want to start masternode %1?\n").arg(strAlias))) {
                    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
                    if (!ctx.isValid()) {
                        // Unlock wallet was cancelled
                        inform(tr("Cannot edit masternode, wallet locked"));
                        return;
                    }
                    startAlias(strAlias);
                }
            } else {
                inform(tr(
                        "Cannot start masternode, the collateral transaction has not been confirmed by the network yet.\n"
                        "Please wait few more minutes (masternode collaterals require %1 confirmations).").arg(
                        mnModel->getMasternodeCollateralMinConf()));
            }
        } else {
            // Deterministic
            bool isEnabled = index.sibling(index.row(), MNModel::IS_POSE_ENABLED).data(Qt::DisplayRole).toBool();
            if (isEnabled) {
                inform(tr("Cannot start an already started Masternode"));
            }
        }
    }
}

void MasterNodesWidget::startAlias(const QString& strAlias)
{
    QString strStatusHtml;
    strStatusHtml += "Alias: " + strAlias + " ";

    int failed_amount = 0;
    int success_amount = 0;
    std::string alias = strAlias.toStdString();
    std::string strError;
    mnModel->startAllLegacyMNs(false, failed_amount, success_amount, &alias, &strError);
    if (failed_amount > 0) {
        strStatusHtml = tr("failed to start.\nError: %1").arg(QString::fromStdString(strError));
    } else if (success_amount > 0) {
        strStatusHtml = tr("successfully started");
    }
    // update UI and notify
    updateModelAndInform(strStatusHtml);
}

void MasterNodesWidget::updateModelAndInform(const QString& informText)
{
    mnModel->updateMNList();
    inform(informText);
}

void MasterNodesWidget::onStartAllClicked(int type)
{
    if (!Params().IsRegTestNet() && !checkMNsNetwork()) return;     // skip on RegNet: so we can test even if tier two not synced

    if (isLoading) {
        inform(tr("Background task is being executed, please wait"));
    } else {
        std::unique_ptr<WalletModel::UnlockContext> pctx = std::make_unique<WalletModel::UnlockContext>(walletModel->requestUnlock());
        if (!pctx->isValid()) {
            warn(tr("Start ALL masternodes failed"), tr("Wallet unlock cancelled"));
            return;
        }
        isLoading = true;
        if (!execute(type, std::move(pctx))) {
            isLoading = false;
            inform(tr("Cannot perform Masternodes start"));
        }
    }
}

bool MasterNodesWidget::startAll(QString& failText, bool onlyMissing)
{
    int amountOfMnFailed = 0;
    int amountOfMnStarted = 0;
    mnModel->startAllLegacyMNs(onlyMissing, amountOfMnFailed, amountOfMnStarted);
    if (amountOfMnFailed > 0) {
        failText = tr("%1 Masternodes failed to start, %2 started").arg(amountOfMnFailed).arg(amountOfMnStarted);
        return false;
    }
    return true;
}

void MasterNodesWidget::run(int type)
{
    bool isStartMissing = type == REQUEST_START_MISSING;
    if (type == REQUEST_START_ALL || isStartMissing) {
        QString failText;
        QString inform = startAll(failText, isStartMissing) ? tr("All Masternodes started!") : failText;
        QMetaObject::invokeMethod(this, "updateModelAndInform", Qt::QueuedConnection,
                                  Q_ARG(QString, inform));
    }

    isLoading = false;
}

void MasterNodesWidget::onError(QString error, int type)
{
    if (type == REQUEST_START_ALL) {
        QMetaObject::invokeMethod(this, "inform", Qt::QueuedConnection,
                                  Q_ARG(QString, "Error starting all Masternodes"));
    }
}

void MasterNodesWidget::onInfoMNClicked()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot show Masternode information, wallet locked"));
        return;
    }
    showHideOp(true);
    MnInfoDialog* dialog = new MnInfoDialog(window);
    QString label = index.data(Qt::DisplayRole).toString();
    QString address = index.sibling(index.row(), MNModel::ADDRESS).data(Qt::DisplayRole).toString();
    QString status = index.sibling(index.row(), MNModel::STATUS).data(Qt::DisplayRole).toString();
    QString txId = index.sibling(index.row(), MNModel::COLLATERAL_ID).data(Qt::DisplayRole).toString();
    QString outIndex = index.sibling(index.row(), MNModel::COLLATERAL_OUT_INDEX).data(Qt::DisplayRole).toString();
    QString pubKey = index.sibling(index.row(), MNModel::PUB_KEY).data(Qt::DisplayRole).toString();
    bool isLegacy = ((uint8_t) index.sibling(index.row(), MNModel::TYPE).data(Qt::DisplayRole).toUInt()) == MNViewType::LEGACY;
    Optional<DMNData> opDMN = nullopt;
    if (!isLegacy) {
        QString proTxHash = index.sibling(index.row(), MNModel::PRO_TX_HASH).data(Qt::DisplayRole).toString();
        opDMN = interfaces::g_tiertwo->getDMNData(uint256S(proTxHash.toStdString()),
                                                  clientModel->getLastBlockIndexProcessed());
    }
    dialog->setData(pubKey, label, address, txId, outIndex, status, opDMN);
    dialog->adjustSize();
    showDialog(dialog, 3, 17);
    if (dialog->exportMN) {
        QString legacyText = isLegacy ? tr(" Then start the Masternode using\nthis controller wallet (select the Masternode in the list and press \"start\").") : "";
        if (ask(tr("Remote Masternode Data"),
                tr("You are just about to export the required data to run a Masternode\non a remote server to your clipboard.\n\n\n"
                   "You will only have to paste the data in the pivx.conf file\nof your remote server and start it."
                   "%1\n").arg(legacyText))) {
            // export data
            QString exportedMN = "masternode=1\n"
                                 "externalip=" + address.left(address.lastIndexOf(":")) + "\n"
                                 "listen=1\n";
            if (isLegacy) {
                exportedMN += "masternodeaddr=" + address + +"\n" +
                             "masternodeprivkey=" +
                             index.sibling(index.row(), MNModel::PRIV_KEY).data(Qt::DisplayRole).toString() + "\n";
            } else {
                exportedMN += "mnoperatorprivatekey=" + QString::fromStdString(opDMN->operatorSk.empty() ? "<insert operator private key here>" : opDMN->operatorSk) + "\n";
            }
            GUIUtil::setClipboard(exportedMN);
            inform(tr("Masternode data copied to the clipboard."));
        }
    }

    dialog->deleteLater();
}

void MasterNodesWidget::onDeleteMNClicked()
{
    QString txId = index.sibling(index.row(), MNModel::COLLATERAL_ID).data(Qt::DisplayRole).toString();
    QString outIndex = index.sibling(index.row(), MNModel::COLLATERAL_OUT_INDEX).data(Qt::DisplayRole).toString();
    QString qAliasString = index.data(Qt::DisplayRole).toString();
    bool isLegacy = ((uint8_t) index.sibling(index.row(), MNModel::TYPE).data(Qt::DisplayRole).toUInt()) == MNViewType::LEGACY;

    bool convertOK = false;
    unsigned int indexOut = outIndex.toUInt(&convertOK);
    if (!convertOK) {
        inform(tr("Invalid collateral output index"));
        return;
    }

    if (isLegacy) {
        if (!ask(tr("Delete Masternode"), tr("You are just about to delete Masternode:\n%1\n\nAre you sure?").arg(qAliasString))) {
            return;
        }

        QString errorStr;
        if (!mnModel->removeLegacyMN(qAliasString.toStdString(), txId.toStdString(), indexOut, errorStr)) {
            inform(errorStr);
            return;
        }
        // Update list
        mnModel->removeMn(index);
        updateListState();
    } else {
        if (!ask(tr("Delete Masternode"), tr("You are just about to spend the collateral\n"
                                             "(creating a transaction to yourself)\n"
                                             "of your Masternode:\n\n%1\n\nAre you sure?")
            .arg(qAliasString))) {
            return;
        }

        auto res = mnModel->killDMN(uint256S(txId.toStdString()), indexOut);
        if (!res) {
            inform(QString::fromStdString(res.getError()));
            return;
        }
        inform("Deterministic Masternode removed successfully! the change will be reflected on the next mined block");
    }
}

void MasterNodesWidget::onCreateMNClicked()
{
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        inform(tr("Cannot create Masternode controller, wallet locked"));
        return;
    }

    CAmount mnCollateralAmount = mnModel->getMNCollateralRequiredAmount();
    if (walletModel->getBalance() <= mnCollateralAmount) {
        inform(tr("Not enough balance to create a masternode, %1 required.")
            .arg(GUIUtil::formatBalance(mnCollateralAmount, BitcoinUnits::PIV)));
        return;
    }
    MasterNodeWizardDialog* dialog = new MasterNodeWizardDialog(walletModel, mnModel, clientModel, window);
    connect(dialog, &MasterNodeWizardDialog::message, this, &PWidget::emitMessage);
    do {
        showHideOp(true);
        dialog->isWaitingForAsk = false;
        if (openDialogWithOpaqueBackgroundY(dialog, window, 5, 7)) {
            if (dialog->isOk) {
                updateListState();
                inform(dialog->returnStr);
            } else {
                warn(tr("Error creating masternode"), dialog->returnStr);
            }
        } else if (dialog->isWaitingForAsk) {
            auto* askDialog = new DefaultDialog(window);
            showHide(true);
            askDialog->setText(tr("Advanced Masternode Configurations"),
                            tr("The wallet can complete the next steps,\ncreating the MN keys and addresses automatically\n\n"
                               "Do you want to customize the owner, operator\nand voter or create them automatically?\n"
                               "(recommended only for advanced users)"),
                               tr("Automatic"), tr("Customize"));
            askDialog->adjustSize();
            openDialogWithOpaqueBackground(askDialog, window);
            askDialog->isOk ? dialog->completeTask() : dialog->moveToAdvancedConf();
        }
    } while (dialog->isWaitingForAsk);

    dialog->deleteLater();
}

void MasterNodesWidget::changeTheme(bool isLightTheme, QString& theme)
{
    static_cast<MNHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
}

MasterNodesWidget::~MasterNodesWidget()
{
    delete ui;
}
