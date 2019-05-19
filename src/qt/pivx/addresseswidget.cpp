#include "qt/pivx/addresseswidget.h"
#include "qt/pivx/forms/ui_addresseswidget.h"
#include "qt/pivx/addresslabelrow.h"
#include "qt/pivx/addnewaddressdialog.h"
#include "qt/pivx/tooltipmenu.h"

#include "qt/pivx/addnewcontactdialog.h"
#include "qt/pivx/PIVXGUI.h"
#include "guiutil.h"
#include "qt/pivx/qtutils.h"
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
    PWidget(_window, parent),
    ui(new Ui::AddressesWidget)
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
        connect(this->menu, &TooltipMenu::message, this, &AddressesWidget::message);
        connect(this->menu, SIGNAL(onEditClicked()), this, SLOT(onEditClicked()));
        connect(this->menu, SIGNAL(onDeleteClicked()), this, SLOT(onDeleteClicked()));
        connect(this->menu, SIGNAL(onCopyClicked()), this, SLOT(onCopyClicked()));
    }else {
        this->menu->hide();
    }
    this->index = rIndex;
    menu->move(pos);
    menu->show();
}

void AddressesWidget::loadWalletModel(){
    if(walletModel) {
        addressTablemodel = walletModel->getAddressTableModel();
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
            inform(tr("Invalid Contact Address"));
            return;
        }

        CBitcoinAddress pivAdd = CBitcoinAddress(address.toUtf8().constData());
        if (walletModel->isMine(pivAdd)) {
            setCssEditLine(ui->lineEditAddress, false, true);
            inform(tr("Cannot store your own address as contact"));
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

            inform(tr("New Contact Stored"));
        } else {
            inform(tr("Error Storing Contact"));
        }

    }
}

void AddressesWidget::onEditClicked(){
    QString address = index.data(Qt::DisplayRole).toString();
    QString currentLabel = index.sibling(index.row(), AddressTableModel::Label).data(Qt::DisplayRole).toString();
    showHideOp(true);
    AddNewContactDialog *dialog = new AddNewContactDialog(window);
    dialog->setData(address, currentLabel);
    if(openDialogWithOpaqueBackground(dialog, window)){
        if(walletModel->updateAddressBookLabels(
                CBitcoinAddress(address.toStdString()).Get(), dialog->getLabel().toStdString(), "send")){
            inform(tr("Contact edited"));
        }else{
            inform(tr("Contact edit failed"));
        }
    }
}

void AddressesWidget::onDeleteClicked(){
    if(walletModel) {
        bool ret = false;
        ask(tr("Delete Contact"),
            tr("You are just about to remove the contact:\n\n%1\n\nAre you sure?").arg(index.data(Qt::DisplayRole).toString().toUtf8().constData()),
            &ret);
        if (ret) {
            if (this->walletModel->getAddressTableModel()->removeRows(index.row(), 1, index)) {
                inform(tr("Contact Deleted"));
            } else {
                inform(tr("Error deleting a contact"));
            }
        }
    }
}

void AddressesWidget::onCopyClicked(){
    GUIUtil::setClipboard(index.data(Qt::DisplayRole).toString());
    inform(tr("Address copied"));
}

void AddressesWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<ContactsHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
}

AddressesWidget::~AddressesWidget(){
    delete ui;
}
