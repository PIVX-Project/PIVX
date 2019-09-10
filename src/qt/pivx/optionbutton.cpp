// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/optionbutton.h"
#include "qt/pivx/forms/ui_optionbutton.h"
#include "qt/pivx/qtutils.h"
#include <QMouseEvent>

OptionButton::OptionButton(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OptionButton)
{
    ui->setupUi(this);
    setCssProperty(ui->labelArrow3, "ic-arrow");
    setCssProperty(ui->layoutOptions2, "container-options");
    ui->layoutOptions2->setContentsMargins(0,10,10,10);
    setCssProperty(ui->labelCircle, "btn-options-indicator");
    connect(ui->labelArrow3, &QPushButton::clicked, [this](){setChecked(!ui->labelArrow3->isChecked());});
    setActive(false);
}

OptionButton::~OptionButton(){
    delete ui;
}

void OptionButton::setTitleClassAndText(QString className, QString text){
    ui->labelTitleChange->setText(text);
    setCssProperty(ui->labelTitleChange, className);
}

void OptionButton::setTitleText(QString text){
    ui->labelTitleChange->setText(text);
}

void OptionButton::setSubTitleClassAndText(QString className, QString text){
    ui->labelSubtitleChange->setText(text);
    setCssProperty(ui->labelSubtitleChange, className);
}

void OptionButton::setRightIconClass(QString className, bool forceUpdate){
    setCssProperty(ui->labelArrow3, className);
    if(forceUpdate) updateStyle(ui->labelArrow3);
}

void OptionButton::setRightIcon(QPixmap icon){
    //ui->labelArrow3->setPixmap(icon);
}

void OptionButton::setActive(bool isActive){
    if (isActive) {
        ui->layoutCircle->setVisible(true);
        setCssProperty(ui->labelTitleChange, "btn-title-purple");
        updateStyle(ui->labelTitleChange);
    } else {
        ui->layoutCircle->setVisible(false);
        setCssProperty(ui->labelTitleChange, "btn-title-grey");
        updateStyle(ui->labelTitleChange);
    }
}

void OptionButton::setChecked(bool checked){
    ui->labelArrow3->setChecked(checked);
    emit clicked();
}

void OptionButton::mousePressEvent(QMouseEvent *qevent){
    if (qevent->button() == Qt::LeftButton){
        setChecked(!ui->labelArrow3->isChecked());
    }
}
