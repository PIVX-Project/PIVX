// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/walletpassworddialog.h"
#include "qt/pivx/forms/ui_walletpassworddialog.h"
#include <QGraphicsDropShadowEffect>

WalletPasswordDialog::WalletPasswordDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WalletPasswordDialog),
    btnWatch(new QCheckBox())
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Text

    ui->labelTitle->setText("Encrypt your wallet");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("Secure your wallet with a strong password that you won’t lose or forget.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    ui->labelPassword->setText("Password");
    ui->labelPassword->setProperty("cssClass", "text-title");


    ui->labelPasswordRepeat->setText("Confirm password");
    ui->labelPasswordRepeat->setProperty("cssClass", "text-title");


    // Edit Passwords

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

    ui->lineEditPassword->setPlaceholderText("At least 6 alphanumerics characters (e.j. Crypt34)");
    ui->lineEditPassword->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditPassword->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditPassword->setEchoMode(QLineEdit::Password);
    ui->stackedWidget_2->setGraphicsEffect(shadowEffect);

    ui->lineEditPasswordRepeat->setPlaceholderText("Repeat the new password");
    ui->lineEditPasswordRepeat->setProperty("cssClass", "edit-primary-dialog");
    ui->lineEditPasswordRepeat->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditPasswordRepeat->setEchoMode(QLineEdit::Password);
    ui->lineEditPasswordRepeat->setGraphicsEffect(shadowEffect2);

    // Show Repeat Password

    ui->layoutRepeat->setVisible(true);


    // Button Watch

    btnWatch->setProperty("cssClass", "btn-watch-password");
    btnWatch->setCheckable(true);
     btnWatch->setChecked(false);
    QSize BUTTON_CONTACT_SIZE = QSize(24, 24);
    btnWatch->setMinimumSize(BUTTON_CONTACT_SIZE);
    btnWatch->setMaximumSize(BUTTON_CONTACT_SIZE);

    ui->stackedWidget_2->addWidget(btnWatch);

    btnWatch->show();
    btnWatch->raise();

    int posXX = ui->lineEditPassword->width() - 30;
    int posYY = 12;
    btnWatch->move(450, posYY);


    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("ENCRYPT");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(btnWatch, SIGNAL(clicked()), this, SLOT(onWatchClicked()));
    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

void WalletPasswordDialog::onWatchClicked(){
    ui->lineEditPassword->setEchoMode(btnWatch->checkState() == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password );
    ui->lineEditPasswordRepeat->setEchoMode(btnWatch->checkState() == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password );
}

WalletPasswordDialog::~WalletPasswordDialog()
{
    delete ui;
}
