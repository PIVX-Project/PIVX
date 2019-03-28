#include "qt/pivx/send.h"
#include "qt/pivx/forms/ui_send.h"

#include "qt/pivx/qtutils.h"
#include "qt/pivx/sendchangeaddressdialog.h"
#include "qt/pivx/optionbutton.h"
#include "qt/pivx/sendcustomfeedialog.h"
#include "qt/pivx/coincontrolzpivdialog.h"
#include "qt/pivx/coincontrolpivwidget.h"
#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/myaddressrow.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>

#include <iostream>

SendWidget::SendWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::send),
    window(_window),
    coinIcon(new QPushButton()),
    btnContacts(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,20,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    QGraphicsDropShadowEffect* shadowEffect3 = new QGraphicsDropShadowEffect();
    shadowEffect3->setColor(QColor(0, 0, 0, 22));
    shadowEffect3->setXOffset(0);
    shadowEffect3->setYOffset(3);
    shadowEffect3->setBlurRadius(6);

    /* Title */
    ui->labelTitle->setText("Send");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText("PIV");
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText("zPIV");
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    /* Subtitle */

    ui->labelSubtitle1->setText("You can transfer public coins: PIV or private ones: zPIV");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle2->setText("Select coin type to spend");
    ui->labelSubtitle2->setProperty("cssClass", "text-subtitle");

    /* Address */

    ui->labelSubtitleAddress->setText("Enter a PIVX address or contact label");
    ui->labelSubtitleAddress->setProperty("cssClass", "text-title");

    ui->lineEditAddress->setPlaceholderText("Add address");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-book-send");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->stackedWidget_2->setGraphicsEffect(shadowEffect);

    /* Amount */

    ui->labelSubtitleAmount->setText("Amount");
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title");

    ui->lineEditAmount->setPlaceholderText("0.00 zPIV ");
    ui->lineEditAmount->setProperty("cssClass", "edit-primary");
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAmount->setGraphicsEffect(shadowEffect2);


    /* Description */

    ui->labelSubtitleDescription->setText("Label address (optional)");
    ui->labelSubtitleDescription->setProperty("cssClass", "text-title");

    ui->lineEditDescription->setPlaceholderText("Add descripcion ");
    ui->lineEditDescription->setProperty("cssClass", "edit-primary");
    ui->lineEditDescription->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDescription->setGraphicsEffect(shadowEffect3);


    // Buttons

    ui->pushButtonFee->setText("Standard Fee 0.000005 PIV");
    ui->pushButtonFee->setProperty("cssClass", "btn-secundary");

    ui->pushButtonClear->setText("Clear all");
    ui->pushButtonClear->setProperty("cssClass", "btn-secundary-clear");

    ui->pushButtonAddRecipient->setText("Add recipient");
    ui->pushButtonAddRecipient->setProperty("cssClass", "btn-secundary-add");

    ui->pushButtonSave->setText("Send zPIV");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");

    // Coin control
    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Control coin");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select the source of the coins.");

    // Change eddress option
    ui->btnChangeAddress->setTitleClassAndText("btn-title-grey", "Change address");
    ui->btnChangeAddress->setSubTitleClassAndText("text-subtitle", "Customize the change address.");

    connect(ui->pushButtonFee, SIGNAL(clicked()), this, SLOT(onChangeCustomFeeClicked()));
    connect(ui->btnCoinControl, SIGNAL(clicked()), this, SLOT(onCoinControlClicked()));
    connect(ui->btnChangeAddress, SIGNAL(clicked()), this, SLOT(onChangeAddressClicked()));

    ui->coinWidget->setProperty("cssClass", "container-coin-type");
    ui->labelLine->setProperty("cssClass", "container-divider");


    // Total Send

    ui->labelTitleTotalSend->setText("Total to send");
    ui->labelTitleTotalSend->setProperty("cssClass", "text-title");

    ui->labelAmountSend->setText("0.0 zPIV");
    ui->labelAmountSend->setProperty("cssClass", "text-body1");

    // Total Remaining

    ui->labelTitleTotalRemaining->setText("Total remaining");
    ui->labelTitleTotalRemaining->setProperty("cssClass", "text-title");

    ui->labelAmountRemaining->setText("1000 zPIV");
    ui->labelAmountRemaining->setProperty("cssClass", "text-body1");

    // Contact Button

    btnContacts->setProperty("cssClass", "btn-dropdown");
    btnContacts->setCheckable(true);

    QSize BUTTON_CONTACT_SIZE = QSize(24, 24);
    btnContacts->setMinimumSize(BUTTON_CONTACT_SIZE);
    btnContacts->setMaximumSize(BUTTON_CONTACT_SIZE);

    ui->stackedWidget_2->addWidget(btnContacts);

    btnContacts->show();
    btnContacts->raise();

    int posXX = ui->lineEditAddress->width() - 30;
    int posYY = 12;
    btnContacts->move(450, posYY);

    // Icon Send
    ui->stackedWidget->addWidget(coinIcon);
    coinIcon->show();
    coinIcon->raise();

    coinIcon->setProperty("cssClass", "coin-icon-zpiv");

    QSize BUTTON_SIZE = QSize(24, 24);
    coinIcon->setMinimumSize(BUTTON_SIZE);
    coinIcon->setMaximumSize(BUTTON_SIZE);

    int posX = 0;
    int posY = 20;
    coinIcon->move(posX, posY);

    // Show multirow
    sendMultiRow = new SendMultiRow(this);
    ui->scrollArea->setWidget(sendMultiRow);

    // Connect
    connect(ui->pushLeft, SIGNAL(clicked()), this, SLOT(onPIVSelected()));
    connect(ui->pushRight, SIGNAL(clicked()), this, SLOT(onzPIVSelected()));
    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(btnContacts, SIGNAL(clicked()), this, SLOT(onContactsClicked()));

    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
}


void SendWidget::resizeEvent(QResizeEvent *event)
 {
    int posXX = ui->lineEditAddress->width() - 30;
    int posYY = 12;
    btnContacts->move(posXX, posYY);
    resizeMenu();
    QWidget::resizeEvent(event);
 }


void SendWidget::onSendClicked(){
    window->showHide(true);
    SendConfirmDialog* dialog = new SendConfirmDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

}


void SendWidget::onChangeAddressClicked(){
    window->showHide(true);
    SendChangeAddressDialog* dialog = new SendChangeAddressDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

}

void SendWidget::onChangeCustomFeeClicked(){
    window->showHide(true);
    SendCustomFeeDialog* dialog = new SendCustomFeeDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);
}

void SendWidget::onCoinControlClicked()
{
    window->showHide(true);
    if(isPIV){
        CoinControlPivWidget* dialog = new CoinControlPivWidget(window);
        openDialogWithOpaqueBackgroundY(dialog, window, 6.5, 5);
    }else{
        CoinControlZpivDialog* dialog = new CoinControlZpivDialog(window);
        openDialogWithOpaqueBackgroundY(dialog, window, 6.5, 5);
    }
}

void SendWidget::onPIVSelected(){
    isPIV = true;
    coinIcon->setProperty("cssClass", "coin-icon-piv");
    updateStyle(coinIcon);
}

void SendWidget::onzPIVSelected(){
    isPIV = false;
    coinIcon->setProperty("cssClass", "coin-icon-zpiv");
    updateStyle(coinIcon);
}


void SendWidget::onContactsClicked(){
    int height = ui->stackedWidget_2->height() * 8;
    int width = ui->stackedWidget_2->width();

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this
                    );

    }else{
        menuContacts->setMinimumHeight(height);
        menuContacts->setMinimumWidth(width);
        menuContacts->resizeList(width, height);
        menuContacts->resize(width, height);
    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }

    menuContacts->setStyleSheet(this->styleSheet());

    QRect rect = ui->lineEditAddress->rect();
    QPoint pos = rect.bottomLeft();
    // TODO: Change this position..
    pos.setX(pos.x() + 20);
    pos.setY(pos.y() + ((ui->lineEditAddress->height() - 4)  * 3));
    menuContacts->move(pos);
    menuContacts->show();
}

void SendWidget::resizeMenu(){
    if(menuContacts && menuContacts->isVisible()){
        int width = ui->stackedWidget_2->width();
        menuContacts->resizeList(width, menuContacts->height());
        menuContacts->resize(width, menuContacts->height());
        QRect rect = ui->lineEditAddress->rect();
        QPoint pos = rect.bottomLeft();
        // TODO: Change this position..
        pos.setX(pos.x() + 20);
        pos.setY(pos.y() + ((ui->lineEditAddress->height() - 4)  * 3));
        menuContacts->move(pos);
    }
}

void SendWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    if(this->menuContacts){
        this->menuContacts->setStyleSheet(theme);
        if(this->menuContacts->isVisible()){
            updateStyle(menuContacts);
        }
    }
    updateStyle(this);
}

SendWidget::~SendWidget()
{
    delete ui;
}
