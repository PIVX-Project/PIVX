// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FOCUSEDDIALOG_H
#define FOCUSEDDIALOG_H

#include <QDialog>

class FocusedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FocusedDialog(QWidget *parent = nullptr);
    ~FocusedDialog();

    // Sets focus on show
    void showEvent(QShowEvent *event);

protected:
    // Detects a key press and calls accept() on ENTER and reject() on ESC
    void keyPressEvent(QKeyEvent *e);
};

#endif // FOCUSEDDIALOG_H
