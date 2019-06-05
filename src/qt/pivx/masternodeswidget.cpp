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


#define DECORATION_SIZE 60
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

        /*ow->updateState(isLightTheme, isHovered, isSelected);

        QString address = index.data(Qt::DisplayRole).toString();
        QModelIndex sibling = index.sibling(index.row(), AddressTableModel::Label);
        QString label = sibling.data(Qt::DisplayRole).toString();

        row->updateView(address, label);
         */
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
    this->setStyleSheet(parent->styleSheet());

    delegate = new FurAbstractListItemDelegate(
            DECORATION_SIZE,
            new MNHolder(isLightTheme()),
            this
    );

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,20);
    ui->right->setProperty("cssClass", "container-right");
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
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    /* Options */
    ui->btnAbout->setTitleClassAndText("btn-title-grey", "About Masternode");
    ui->btnAbout->setSubTitleClassAndText("text-subtitle", "Select the source of the coins for your transaction.");

    // hide list.
    ui->listMn->setVisible(false);

    //ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-master");

    ui->labelEmpty->setText(tr("No active Masternode yet"));
    ui->labelEmpty->setProperty("cssClass", "text-empty");

    // Connect btns
    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onCreateMNClicked()));
}

void MasterNodesWidget::loadWalletModel(){
    if(walletModel) {

    }
}

void MasterNodesWidget::onCreateMNClicked(){
    /*
    MasterNodeWizardDialog* dialog = new MasterNodeWizardDialog(mainWindow);
    mainWindow->showHide(true);
    openDialogWithOpaqueBackgroundY(dialog, mainWindow, 5, 7);
     */
}

void MasterNodesWidget::changeTheme(bool isLightTheme, QString& theme){
}

MasterNodesWidget::~MasterNodesWidget()
{
    delete ui;
}
