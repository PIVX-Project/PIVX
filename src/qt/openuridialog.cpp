// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "openuridialog.h"
#include "ui_openuridialog.h"

#include "guiutil.h"
#include "walletmodel.h"
#include "qt/pivx/qtutils.h"

#include <QUrl>

OpenURIDialog::OpenURIDialog(QWidget* parent) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                ui(new Ui::OpenURIDialog)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());
    ui->uriEdit->setPlaceholderText("pivx:");



    ui->labelSubtitle->setText("URI");
    ui->labelSubtitle->setProperty("cssClass", "text-title2-dialog");

    ui->frame->setProperty("cssClass", "container-dialog");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->selectFileButton->setProperty("cssClass", "btn-primary");
    

    ui->uriEdit->setPlaceholderText("0.000001 zPIV");
    ui->uriEdit->setProperty("cssClass", "edit-primary-dialog");
    ui->uriEdit->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->uriEdit);

}

OpenURIDialog::~OpenURIDialog()
{
    delete ui;
}

QString OpenURIDialog::getURI()
{
    return ui->uriEdit->text();
}

void OpenURIDialog::accept()
{
    SendCoinsRecipient rcp;
    if (GUIUtil::parseBitcoinURI(getURI(), &rcp)) {
        /* Only accept value URIs */
        QDialog::accept();
    } else {
        ui->uriEdit->setValid(false);
    }
}

void OpenURIDialog::on_selectFileButton_clicked()
{
    QString filename = GUIUtil::getOpenFileName(this, tr("Select payment request file to open"), "", "", NULL);
    if (filename.isEmpty())
        return;
    QUrl fileUri = QUrl::fromLocalFile(filename);
    ui->uriEdit->setText("pivx:?r=" + QUrl::toPercentEncoding(fileUri.toString()));
}
