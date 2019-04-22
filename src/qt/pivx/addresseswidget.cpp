#include "qt/pivx/addresseswidget.h"
#include "qt/pivx/forms/ui_addresseswidget.h"
#include "qt/pivx/addresslabelrow.h"
#include "qt/pivx/addnewaddressdialog.h"
#include "qt/pivx/tooltipmenu.h"


#include "qt/pivx/PIVXGUI.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/snackbar.h"
#include "walletmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QModelIndex>
#include <QFile>
#include <QGraphicsDropShadowEffect>

#include <iostream>

#define DECORATION_SIZE 60
#define NUM_ITEMS 3

class ContactsHolder : public FurListRow<QWidget*>
{
public:
    ContactsHolder();

    explicit ContactsHolder(bool _isLightTheme) : FurListRow(), isLightTheme(_isLightTheme){}

    AddressLabelRow* createHolder(int pos) override{
        return new AddressLabelRow(isLightTheme, false);
    }

    void init(QWidget* holder,const QModelIndex &index, bool isHovered, bool isSelected) const override{
        AddressLabelRow* row = static_cast<AddressLabelRow*>(holder);

        row->updateState(isLightTheme, isHovered, isSelected);

        QString address = index.data(Qt::DisplayRole).toString();
        QModelIndex sibling = index.sibling(index.row(), AddressTableModel::Label);
        QString label = sibling.data(Qt::DisplayRole).toString();

        row->updateView(address, label);
    }

    QColor rectColor(bool isHovered, bool isSelected) override{
        return getRowColor(isLightTheme, isHovered, isSelected);
    }

    ~ContactsHolder() override{}

    bool isLightTheme;
};

#include "qt/pivx/moc_addresseswidget.cpp"

AddressesWidget::AddressesWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddressesWidget),
    window(_window)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    delegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                new ContactsHolder(isLightTheme()),
                this
    );

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    ui->listAddresses->setProperty("cssClass", "container");

    // Title
    ui->labelTitle->setText("Contacts");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    /* Subtitle */

    ui->labelSubtitle1->setText("You can add a new one in the options menu to the side.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Change eddress option
    ui->btnAddContact->setTitleClassAndText("btn-title-grey", "Add new contact");
    ui->btnAddContact->setSubTitleClassAndText("text-subtitle", "Generate a new address to receive tokens.");
    ui->btnAddContact->setRightIconClass("ic-arrow-down");

    // List Addresses

    ui->listAddresses->setItemDelegate(delegate);
    ui->listAddresses->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listAddresses->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listAddresses->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listAddresses->setSelectionBehavior(QAbstractItemView::SelectRows);
    //ui->listAddresses->setVisible(false);


    //Empty List

    ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-contacts");

    ui->labelEmpty->setText("No contacts yet");
    ui->labelEmpty->setProperty("cssClass", "text-empty");


    // Add Contact

    ui->layoutNewContact->setProperty("cssClass", "container-options");


    // Name

    ui->labelName->setText("Contact name");
    ui->labelName->setProperty("cssClass", "text-title");


    ui->lineEditName->setPlaceholderText("e.g John doe ");
    setCssEditLine(ui->lineEditName, true);
    ui->lineEditName->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditName);


    // Address

    ui->labelAddress->setText("Enter a PIVX address");
    ui->labelAddress->setProperty("cssClass", "text-title");

    ui->lineEditAddress->setPlaceholderText("e.g D7VFR83SQbiezrW72hjc…");
    setCssEditLine(ui->lineEditAddress, true);
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditAddress);


    // Buttons

    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->listAddresses, SIGNAL(clicked(QModelIndex)), this, SLOT(handleAddressClicked(QModelIndex)));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(onStoreContactClicked()));
    // style
    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
}

void AddressesWidget::handleAddressClicked(const QModelIndex &index){
    ui->listAddresses->setCurrentIndex(index);
    QRect rect = ui->listAddresses->visualRect(index);
    QPoint pos = rect.topRight();
    pos.setX(pos.x() - (DECORATION_SIZE * 2));
    pos.setY(pos.y() + (DECORATION_SIZE));

    QModelIndex rIndex = filter->mapToSource(index);

    if(!this->menu){
        this->menu = new TooltipMenu(window, this);
        this->menu->setWalletModel(walletModel);
        connect(this->menu, SIGNAL(message(QString, QString, unsigned int, bool* ret)), this, SIGNAL(message(QString, QString, unsigned int, bool* ret)));
    }else {
        this->menu->hide();
        // TODO: update view..
    }
    this->menu->setIndex(rIndex);
    menu->move(pos);
    menu->show();
}

void AddressesWidget::setWalletModel(WalletModel *model){
    this->walletModel = model;
    if(model) {
        addressTablemodel = model->getAddressTableModel();
        this->filter = new AddressFilterProxyModel(AddressTableModel::Send, this);
        this->filter->setSourceModel(addressTablemodel);
        ui->listAddresses->setModel(this->filter);
        ui->listAddresses->setModelColumn(AddressTableModel::Address);


        if(addressTablemodel->sizeSend() == 0){
            ui->emptyContainer->setVisible(true);
            ui->listAddresses->setVisible(false);
        }else{
            ui->emptyContainer->setVisible(false);
            ui->listAddresses->setVisible(true);
        }
    }
}

void AddressesWidget::onStoreContactClicked(){
    if (walletModel) {
        QString label = ui->lineEditName->text();
        QString address = ui->lineEditAddress->text();

        // TODO: Update address status on text change..
        if (!walletModel->validateAddress(address)) {
            setCssEditLine(ui->lineEditAddress, false, true);
            emit message("", tr("Invalid Contact Address"), CClientUIInterface::MSG_INFORMATION_SNACK, nullptr);
            return;
        }

        CBitcoinAddress pivAdd = CBitcoinAddress(address.toUtf8().constData());
        if (walletModel->isMine(pivAdd)) {
            setCssEditLine(ui->lineEditAddress, false, true);
            emit message("", tr("Cannot store your own address as contact"), CClientUIInterface::MSG_INFORMATION_SNACK, nullptr);
            return;
        }

        if (walletModel->updateAddressBookLabels(pivAdd.Get(), label.toUtf8().constData(), "send")) {
            // TODO: Complete me..
            ui->lineEditAddress->setText("");
            ui->lineEditName->setText("");
            setCssEditLine(ui->lineEditAddress, true, true);
            setCssEditLine(ui->lineEditName, true, true);

            if (ui->emptyContainer->isVisible()) {
                ui->emptyContainer->setVisible(false);
                ui->listAddresses->setVisible(true);
            }

            emit message("", tr("New Contact Stored"), CClientUIInterface::MSG_INFORMATION_SNACK, nullptr);
        } else {
            emit message("", tr("Error Storing Contact"), CClientUIInterface::MSG_INFORMATION_SNACK, nullptr);
        }

    }
}

void AddressesWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    static_cast<ContactsHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    updateStyle(this);
}

AddressesWidget::~AddressesWidget(){
    delete ui;
}
