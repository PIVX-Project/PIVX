#include "qt/pivx/receivewidget.h"
#include "qt/pivx/forms/ui_receivewidget.h"
#include "qt/pivx/requestdialog.h"
//#include "qt/pivx/addnewcontactdialog.h"
#include "qt/pivx/qtutils.h"

#include "qt/pivx/PIVXGUI.h"
#include "qt/pivx/myaddressrow.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/furlistrow.h"
#include "walletmodel.h"
#include "guiutil.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QModelIndex>
#include <QFile>
#include <QClipboard>

#include <iostream>

#define DECORATION_SIZE 70
#define NUM_ITEMS 3

class AddressHolder : public FurListRow<QWidget*>
{
public:
    AddressHolder();

    explicit AddressHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    MyAddressRow* createHolder(int pos) override{
        return new MyAddressRow();
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override{
        MyAddressRow *row = static_cast<MyAddressRow*>(holder);

        QString address = index.data(Qt::DisplayRole).toString();
        QModelIndex sibling = index.siblingAtColumn(AddressTableModel::Label);
        QString label = sibling.data(Qt::DisplayRole).toString();

        row->updateView(address, label);

    }

    QColor rectColor(bool isHovered, bool isSelected) override{
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~AddressHolder() override{}

    bool isLightTheme;
};

#include "qt/pivx/moc_receivewidget.cpp"

ReceiveWidget::ReceiveWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReceiveWidget),
    window(_window)
{
    ui->setupUi(this);

    delegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                new AddressHolder(isLightTheme()),
                this
                );

    // Stylesheet

    this->setStyleSheet(_window->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(20,20,20,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(0,9,0,0);


    // Title
    ui->labelTitle->setText("Receive");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->labelSubtitle1->setText("Scan the QR code or copy the address to receive PIV.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");


    // Address

    ui->labelAddress->setText("D7VFR83SQbiezrW72hjcWJtcfip5krte2Z ");
    ui->labelAddress->setProperty("cssClass", "label-address-box");

    ui->labelDate->setText("Dec. 19, 2018");
    ui->labelDate->setProperty("cssClass", "text-subtitle");

    // Options

    ui->btnMyAddresses->setTitleClassAndText("btn-title-grey", "My Addresses");
    ui->btnMyAddresses->setSubTitleClassAndText("text-subtitle", "List your own addresses.");
    ui->btnMyAddresses->layout()->setMargin(0);
    ui->btnMyAddresses->setRightIconClass("btn-dropdown");


    ui->btnRequest->setTitleClassAndText("btn-title-grey", "Create Request");
    ui->btnRequest->setSubTitleClassAndText("text-subtitle", "Request payment with a fixed amount.");
    ui->btnRequest->layout()->setMargin(0);


    // Buttons
    connect(ui->btnRequest, SIGNAL(clicked()), this, SLOT(onRequestClicked()));
    connect(ui->btnMyAddresses, SIGNAL(clicked()), this, SLOT(onMyAddressesClicked()));


    ui->pushButtonLabel->setText("Add Label");
    ui->pushButtonLabel->setProperty("cssClass", "btn-secundary-label");

    ui->pushButtonNewAddress->setText("Generate New Address");
    ui->pushButtonNewAddress->setProperty("cssClass", "btn-secundary-new-address");

    ui->pushButtonCopy->setText("Copy");
    ui->pushButtonCopy->setProperty("cssClass", "btn-secundary-copy");


    // List Addresses
    ui->listViewAddress->setProperty("cssClass", "container");
    ui->listViewAddress->setItemDelegate(delegate);
    ui->listViewAddress->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listViewAddress->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listViewAddress->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listViewAddress->setSelectionBehavior(QAbstractItemView::SelectRows);


    spacer = new QSpacerItem(40, 20, QSizePolicy::Maximum, QSizePolicy::Expanding);
    ui->btnMyAddresses->setChecked(true);
    ui->container_right->addItem(spacer);
    ui->listViewAddress->setVisible(false);

    // Connect
    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
    connect(ui->pushButtonLabel, SIGNAL(clicked()), this, SLOT(onLabelClicked()));
    connect(ui->pushButtonCopy, SIGNAL(clicked()), this, SLOT(onCopyClicked()));
}

void ReceiveWidget::setWalletModel(WalletModel* model){
    this->walletModel = model;
    if(walletModel) {
        this->addressTableModel = model->getAddressTableModel();
        ui->listViewAddress->setModel(this->addressTableModel);
        ui->listViewAddress->setModelColumn(AddressTableModel::Address);

        QString latestAddress = this->addressTableModel->getLastUnusedAddress();
        if(!info) info = new SendCoinsRecipient();
        ui->labelAddress->setText(!latestAddress.isEmpty() ? latestAddress : tr("No address"));
        updateQr(latestAddress);
    }
}

void ReceiveWidget::updateQr(QString address){
    info->address = address;
    QString uri = GUIUtil::formatBitcoinURI(*info);
    ui->labelQrImg->setText("");

    QString error;
    QPixmap pixmap = encodeToQr(uri, error);
    if(!pixmap.isNull()){
        qrImage = &pixmap;
        ui->labelQrImg->setPixmap(qrImage->scaled(ui->labelQrImg->width(), ui->labelQrImg->height()));
    }else{
        ui->labelQrImg->setText(!error.isEmpty() ? error : "Error encoding address");
    }
}

void ReceiveWidget::onLabelClicked(){
    // TODO: CHeck this..
    //window->showHide(true);
    //AddNewContactDialog* dialog = new AddNewContactDialog(window);
    //openDialogWithOpaqueBackgroundY(dialog, window, 3.5, 6);
}

void ReceiveWidget::onCopyClicked(){
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(*info));
    // TODO: Add snackbar..
    //openToastDialog("Address copied", mainWindow->getGUI());
}


void ReceiveWidget::onRequestClicked(){
    window->showHide(true);
    RequestDialog* dialog = new RequestDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3.5, 12);
}

void ReceiveWidget::onMyAddressesClicked(){
    bool isVisible = ui->listViewAddress->isVisible();

    if(!isVisible){
        ui->listViewAddress->setVisible(true);
        ui->container_right->removeItem(spacer);
        ui->listViewAddress->update();
    }else{
        ui->container_right->addItem(spacer);
        ui->listViewAddress->setVisible(false);
    }
}

void ReceiveWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    static_cast<AddressHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    updateStyle(this);
}

ReceiveWidget::~ReceiveWidget()
{
    delete ui;
}
