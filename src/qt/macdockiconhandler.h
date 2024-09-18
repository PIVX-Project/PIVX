// Copyright (c) 2011-2020 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_MACDOCKICONHANDLER_H
#define PIVX_QT_MACDOCKICONHANDLER_H

#include <QMainWindow>
#include <QObject>

/** Macintosh-specific dock icon handler.
 */
class MacDockIconHandler : public QObject
{
    Q_OBJECT

public:
    static MacDockIconHandler* instance();
    static void cleanup();

Q_SIGNALS:
    void dockIconClicked();

private:
    MacDockIconHandler();
};

#endif // PIVX_QT_MACDOCKICONHANDLER_H
