#include "qt/pivx/masternodeswidget.h"
#include "qt/pivx/forms/ui_masternodeswidget.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/mnrow.h"

#include "activemasternode.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "init.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "sync.h"
#include "wallet/wallet.h"
#include "walletmodel.h"
#include "askpassphrasedialog.h"

#include <QMessageBox>
#include <QTimer>

#include <iostream>

#define DECORATION_SIZE 65
#define NUM_ITEMS 3

class MNHolder : public FurListRow<QWidget*>
{
public:
    MNHolder();

    explicit MNHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    MNRow* createHolder(int pos) override{
        return new MNRow();
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override{
        MNRow* row = static_cast<MNRow*>(holder);
        QString label = index.data(Qt::DisplayRole).toString();
        QString address = index.sibling(index.row(), MNModel::ADDRESS).data(Qt::DisplayRole).toString();
        row->updateView("Address: " + address, label);
    }

    QColor rectColor(bool isHovered, bool isSelected) override{
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~MNHolder() override{}

    bool isLightTheme;
};

#include "qt/pivx/moc_masternodeswidget.cpp"

MasterNodesWidget::MasterNodesWidget(PIVXGUI *parent) :
    PWidget(parent),
    ui(new Ui::MasterNodesWidget)
{
    ui->setupUi(this);

    delegate = new FurAbstractListItemDelegate(
            DECORATION_SIZE,
            new MNHolder(isLightTheme()),
            this
    );
    mnModel = new MNModel(this);

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
    ui->labelTitle->setText(tr("Master Node"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Subtitle */
    ui->labelSubtitle1->setText(tr("Minting zPIV anonymizes your PIV by removing transaction history,\nmaking transactions untraceable "));
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    /* Buttons */
    ui->pushButtonSave->setText(tr("Create Master Node"));
    setCssBtnPrimary(ui->pushButtonSave);

    /* Options */
    ui->btnAbout->setTitleClassAndText("btn-title-grey", "About Masternode");
    ui->btnAbout->setSubTitleClassAndText("text-subtitle", "Select the source of the coins for your transaction.");

    setCssProperty(ui->listMn, "container");
    ui->listMn->setItemDelegate(delegate);
    ui->listMn->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listMn->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listMn->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listMn->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->emptyContainer->setVisible(false);
    setCssProperty(ui->pushImgEmpty, "img-empty-master");
    ui->labelEmpty->setText(tr("No active Masternode yet"));
    setCssProperty(ui->labelEmpty, "text-empty");

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onCreateMNClicked()));
    connect(ui->listMn, SIGNAL(clicked(QModelIndex)), this, SLOT(onMNClicked(QModelIndex)));
}

void MasterNodesWidget::loadWalletModel(){
    if(walletModel) {
        ui->listMn->setModel(mnModel);
        ui->listMn->setModelColumn(AddressTableModel::Label);
        if (mnModel->rowCount() > 0) {
            ui->listMn->setVisible(true);
            ui->emptyContainer->setVisible(false);
        } else {
            ui->listMn->setVisible(false);
            ui->emptyContainer->setVisible(true);
        }
    }
}

void MasterNodesWidget::onMNClicked(const QModelIndex &index){
    ui->listMn->setCurrentIndex(index);
    QRect rect = ui->listMn->visualRect(index);
    QPoint pos = rect.topRight();
    pos.setX(pos.x() - (DECORATION_SIZE * 2));
    pos.setY(pos.y() + (DECORATION_SIZE));
    if(!this->menu){
        this->menu = new TooltipMenu(window, this);
        this->menu->setEditBtnText(tr("Start"));
        this->menu->setDeleteBtnText(tr("Delete"));
        this->menu->setCopyBtnVisible(false);
        connect(this->menu, &TooltipMenu::message, this, &AddressesWidget::message);
        connect(this->menu, SIGNAL(onEditClicked()), this, SLOT(onEditMNClicked()));
        connect(this->menu, SIGNAL(onDeleteClicked()), this, SLOT(onDeleteMNClicked()));
        this->menu->adjustSize();
    }else {
        this->menu->hide();
    }
    this->index = index;
    menu->move(pos);
    menu->show();
}

void MasterNodesWidget::onEditMNClicked(){
    if(walletModel) {
        // Start MN
        QString strAlias = this->index.data(Qt::DisplayRole).toString();
        bool ret;
        ask(tr("Start Master Node"), tr("Are you sure you want to start masternode %1?").arg(strAlias), &ret);

        if (ret) {
            WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

            if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
                WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full));

                if (!ctx.isValid()) return; // Unlock wallet was cancelled

                startAlias(strAlias);
                return;
            }

            startAlias(strAlias);
        }
    }
}

void MasterNodesWidget::startAlias(QString strAlias)
{
    QString strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        if (mne.getAlias() == strAlias.toStdString()) {
            std::string strError;
            CMasternodeBroadcast mnb;

            bool fSuccess = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            if (fSuccess) {
                strStatusHtml += "<br>Successfully started masternode.";
                mnodeman.UpdateMasternodeList(mnb);
                mnb.Relay();
            } else {
                strStatusHtml += "<br>Failed to start masternode.<br>Error: " + QString::fromStdString(strError);
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    inform(strStatusHtml);

    // TODO: Update MN list.
}

void MasterNodesWidget::onDeleteMNClicked(){
    // TODO: Remove Master Node unlocking the balance.
}

void MasterNodesWidget::onCreateMNClicked(){
    /*
    MasterNodeWizardDialog* dialog = new MasterNodeWizardDialog(mainWindow);
    mainWindow->showHide(true);
    openDialogWithOpaqueBackgroundY(dialog, mainWindow, 5, 7);
     */
}

void MasterNodesWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<MNHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
}

MasterNodesWidget::~MasterNodesWidget()
{
    delete ui;
}
