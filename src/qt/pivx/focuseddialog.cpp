// Copyright (c) 2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/focuseddialog.h"
#include <QKeyEvent>

FocusedDialog::FocusedDialog(QWidget *parent) :
    QDialog(parent)
{}

void FocusedDialog::showEvent(QShowEvent *event)
{
    setFocus();
}

void FocusedDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        // Detect Enter key press
        if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) accept();
        // Detect Esc key press
        if (ke->key() == Qt::Key_Escape) reject();
    }
}

FocusedDialog::~FocusedDialog()
{}
