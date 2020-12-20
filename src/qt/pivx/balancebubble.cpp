// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/balancebubble.h"
#include "qt/pivx/forms/ui_balancebubble.h"

#include "qt/pivx/qtutils.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <qt/bitcoinunits.h>

BalanceBubble::BalanceBubble(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::BalanceBubble)
{
    ui->setupUi(this);

    ui->frame->setProperty("cssClass", "container-popup");
    setCssProperty({ui->textTransparent, ui->textShield}, "amount-small-popup");

    std::initializer_list<QWidget*> lblTitles = {ui->lblFirst, ui->lblSecond};
    setCssProperty(lblTitles, "text-title-topbar");
    QFont font;
    font.setWeight(QFont::Light);
    for (QWidget* w : lblTitles) { w->setFont(font); }
}

void BalanceBubble::updateValues(int64_t nTransparentBalance, int64_t nShieldBalance, int unit){

    ui->textTransparent->setText(BitcoinUnits::formatWithUnit(unit, nTransparentBalance, false, BitcoinUnits::separatorAlways));
    ui->textShield->setText(BitcoinUnits::formatWithUnit(unit, nShieldBalance, false, BitcoinUnits::separatorAlways));
}

void BalanceBubble::showEvent(QShowEvent *event)
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(400);
    a->setStartValue(0.1);
    a->setEndValue(1);
    a->setEasingCurve(QEasingCurve::InBack);
    a->start(QPropertyAnimation::DeleteWhenStopped);

    if (!hideTimer) hideTimer = new QTimer(this);
    connect(hideTimer, &QTimer::timeout, this, &BalanceBubble::hideTimeout);
    hideTimer->start(7000);
}

void BalanceBubble::hideEvent(QHideEvent *event)
{
    if (hideTimer) hideTimer->stop();
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(800);
    a->setStartValue(1);
    a->setEndValue(0);
    a->setEasingCurve(QEasingCurve::OutBack);
    a->start(QPropertyAnimation::DeleteWhenStopped);
}

void BalanceBubble::hideTimeout()
{
    hide();
}

BalanceBubble::~BalanceBubble()
{
    delete ui;
}