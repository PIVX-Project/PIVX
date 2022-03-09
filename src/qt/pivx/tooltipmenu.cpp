// Copyright (c) 2019-2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/tooltipmenu.h"

#include "qt/pivx/qtutils.h"
#include <QTimer>

TooltipMenu::TooltipMenu(PIVXGUI* _window, QWidget* parent) :
    PWidget(_window, parent)
{
    setupUi();
    setCssProperty(container, "container-list-menu");
}

QPushButton* TooltipMenu::createBtn(const QString& label)
{
    auto* btn = new QPushButton(container);
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(btn->sizePolicy().hasHeightForWidth());
    btn->setSizePolicy(sizePolicy);
    btn->setMinimumSize(QSize(0, 30));
    btn->setMaximumSize(QSize(16777215, 30));
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setText(label);
    setCssProperty({btn}, "btn-list-menu");
    return btn;
}

void TooltipMenu::setupUi()
{
    auto* parent = dynamic_cast<QWidget*>(this);
    if (parent->objectName().isEmpty())
        parent->setObjectName(QStringLiteral("TooltipMenu"));
    parent->resize(90, 110);
    parent->setMinimumSize(QSize(90, 110));
    parent->setMaximumSize(QSize(16777215, 140));
    containerLayout = new QVBoxLayout(parent);
    containerLayout->setSpacing(0);
    containerLayout->setObjectName(QStringLiteral("containerLayout"));
    containerLayout->setContentsMargins(0, 0, 0, 0);
    container = new QWidget(parent);
    container->setObjectName(QStringLiteral("container"));
    verticalLayoutBtns = new QVBoxLayout(container);
    verticalLayoutBtns->setSpacing(0);
    verticalLayoutBtns->setObjectName(QStringLiteral("verticalLayoutBtns"));
    verticalLayoutBtns->setContentsMargins(10, 0, 0, 0);
    containerLayout->addWidget(container);
}

void TooltipMenu::showHideBtn(uint8_t id, bool show)
{
    auto it = mapBtns.find(id);
    if (it != mapBtns.end()) {
        it.value()->setVisible(show);
    }
}

void TooltipMenu::updateLabelForBtn(uint8_t id, const QString& btnLabel)
{
    auto it = mapBtns.find(id);
    if (it != mapBtns.end()) {
        it.value()->setText(btnLabel);
    }
}

void TooltipMenu::setBtnCheckable(uint8_t id, bool checkable, bool isChecked)
{
    auto it = mapBtns.find(id);
    if (it != mapBtns.end()) {
        auto btn = it.value();
        btn->setCheckable(checkable);
        btn->setChecked(isChecked);
    }
}

void TooltipMenu::hideTooltip()
{
    dynamic_cast<QWidget*>(this)->hide();
}

void TooltipMenu::showEvent(QShowEvent *event){
    QTimer::singleShot(5000, this, &TooltipMenu::hideTooltip);
}
