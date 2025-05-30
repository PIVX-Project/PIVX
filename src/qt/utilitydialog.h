// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2017-2020 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_UTILITYDIALOG_H
#define PIVX_QT_UTILITYDIALOG_H

#include <QDialog>
#include <QObject>
#include <QMainWindow>

class ClientModel;

namespace Ui
{
class HelpMessageDialog;
}

/** "Help message" dialog box */
class HelpMessageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpMessageDialog(QWidget* parent, bool about);
    ~HelpMessageDialog();

    void printToConsole();
    void showOrPrint();

private:
    Ui::HelpMessageDialog* ui;
    QString text;
};


/** "Shutdown" window */
class ShutdownWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ShutdownWindow(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);
    static void showShutdownWindow(QMainWindow* window);

protected:
    void closeEvent(QCloseEvent* event);
};

#endif // PIVX_QT_UTILITYDIALOG_H
