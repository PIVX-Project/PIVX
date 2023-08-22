// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIUTIL_H
#define BITCOIN_QT_GUIUTIL_H

#include "amount.h"
#include "askpassphrasedialog.h"
#include "fs.h"

#include <QEvent>
#include <QHeaderView>
#include <QMessageBox>
#include <QObject>
#include <QProgressBar>
#include <QString>
#include <QTableView>
#include <QTableWidget>


class QValidatedLineEdit;
class SendCoinsRecipient;

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QDateTime;
class QFont;
class QLineEdit;
class QUrl;
class QWidget;
QT_END_NAMESPACE

/*
 * General GUI exception
 */
class GUIException : public std::exception
{
public:
    std::string message;
    GUIException(const std::string &message) : message(message) {}
};

/** Utility functions used by the hemis Qt UI.
 */
namespace GUIUtil
{
// Create human-readable string from date
QString dateTimeStr(const QDateTime& datetime);
QString dateTimeStrWithSeconds(const QDateTime& date);
QString dateTimeStr(qint64 nTime);

// Render hemis addresses in monospace font
QFont bitcoinAddressFont();

// Parse string into a CAmount value.
// Return 0 if the value is invalid
CAmount parseValue(const QString& amount, int displayUnit = 0);

// Format an amount
QString formatBalance(CAmount amount, int nDisplayUnit = 0, bool isZpiv = false);
QString formatBalanceWithoutHtml(CAmount amount, int nDisplayUnit = 0, bool isZpiv = false);


// Set up widgets for address and amounts
void setupAddressWidget(QValidatedLineEdit* widget, QWidget* parent);
void setupAmountWidget(QLineEdit* widget, QWidget* parent);

// Update the cursor of the widget after a text change
void updateWidgetTextAndCursorPosition(QLineEdit* widget, const QString& str);

// Parse "hemis:" URI into recipient object, return true on successful parsing
bool parseBitcoinURI(const QUrl& uri, SendCoinsRecipient* out);
bool parseBitcoinURI(QString uri, SendCoinsRecipient* out);
QString formatBitcoinURI(const SendCoinsRecipient& info);

// Returns true if given address+amount meets "dust" definition
bool isDust(const QString& address, const CAmount& amount);

// HTML escaping for rich text controls
QString HtmlEscape(const QString& str, bool fMultiLine = false);
QString HtmlEscape(const std::string& str, bool fMultiLine = false);

/** Copy a field of the currently selected entry of a view to the clipboard. Does nothing if nothing
        is selected.
       @param[in] column  Data column to extract from the model
       @param[in] role    Data role to extract from the model
       @see  TransactionView::copyLabel, TransactionView::copyAmount, TransactionView::copyAddress
     */
void copyEntryData(QAbstractItemView* view, int column, int role = Qt::EditRole);

/** Return a field of the currently selected entry as a QString. Does nothing if nothing
        is selected.
       @param[in] column  Data column to extract from the model
       @param[in] role    Data role to extract from the model
       @see  TransactionView::copyLabel, TransactionView::copyAmount, TransactionView::copyAddress
     */
QVariant getEntryData(QAbstractItemView *view, int column, int role);

void setClipboard(const QString& str);

/** Get save filename, mimics QFileDialog::getSaveFileName, except that it appends a default suffix
        when no suffix is provided by the user.

      @param[in] parent  Parent window (or 0)
      @param[in] caption Window caption (or empty, for default)
      @param[in] dir     Starting directory (or empty, to default to documents directory)
      @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
      @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that was selected (or 0).
                  Can be useful when choosing the save file format based on suffix.
     */
QString getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, QString* selectedSuffixOut);

/** Get open filename, convenience wrapper for QFileDialog::getOpenFileName.

      @param[in] parent  Parent window (or 0)
      @param[in] caption Window caption (or empty, for default)
      @param[in] dir     Starting directory (or empty, to default to documents directory)
      @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
      @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that was selected (or 0).
                  Can be useful when choosing the save file format based on suffix.
     */
QString getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, QString* selectedSuffixOut);

/** Get connection type to call object slot in GUI thread with invokeMethod. The call will be blocking.

       @returns If called from the GUI thread, return a Qt::DirectConnection.
                If called from another thread, return a Qt::BlockingQueuedConnection.
    */
Qt::ConnectionType blockingGUIThreadConnection();

// Activate, show and raise the widget
void bringToFront(QWidget* w);

// Determine whether a widget is hidden behind other windows
bool isObscured(QWidget* w);

// Open debug.log
bool openDebugLogfile();

// Open hemis.conf
bool openConfigfile();

// Open gamemaster.conf
bool openGMConfigfile();

// Browse backup folder
bool showBackups();

/** Qt event filter that intercepts ToolTipChange events, and replaces the tooltip with a rich text
      representation if needed. This assures that Qt can word-wrap long tooltip messages.
      Tooltips longer than the provided size threshold (in characters) are wrapped.
     */
class ToolTipToRichTextFilter : public QObject
{
    Q_OBJECT

public:
    explicit ToolTipToRichTextFilter(int size_threshold, QObject* parent = 0);

protected:
    bool eventFilter(QObject* obj, QEvent* evt);

private:
    int size_threshold;
};

bool GetStartOnSystemStartup();
bool SetStartOnSystemStartup(bool fAutoStart);

/** Save window size and position */
void saveWindowGeometry(const QString& strSetting, QWidget* parent);
/** Restore window size and position */
void restoreWindowGeometry(const QString& strSetting, const QSize& defaultSizeIn, QWidget* parent);

/** Load global CSS theme */
QString loadStyleSheet();

/** Check whether a theme is not build-in */
bool isExternal(QString theme);

/* Convert QString to OS specific boost path through UTF-8 */
fs::path qstringToBoostPath(const QString& path);

/* Convert OS specific boost path to QString through UTF-8 */
QString boostPathToQString(const fs::path& path);

/* Convert seconds into a QString with days, hours, mins, secs */
QString formatDurationStr(int secs);

/* Format CNodeStats.nServices bitmask into a user-readable string */
QString formatServicesStr(quint64 mask);

/* Format a CNodeCombinedStats.dPingTime into a user-readable string or display N/A, if 0*/
QString formatPingTime(double dPingTime);

/* Format a CNodeCombinedStats.nTimeOffset into a user-readable string. */
QString formatTimeOffset(int64_t nTimeOffset);

typedef QProgressBar ProgressBar;

/**
* Splits the string into substrings wherever separator occurs, and returns
* the list of those strings. Empty strings do not appear in the result.
*
* QString::split() signature differs in different Qt versions:
*  - QString::SplitBehavior is deprecated since Qt 5.15
*  - Qt::SplitBehavior was introduced in Qt 5.14
* If {QString|Qt}::SkipEmptyParts behavior is required, use this
* function instead of QString::split().
*/
template <typename SeparatorType>
QStringList SplitSkipEmptyParts(const QString& string, const SeparatorType& separator)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return string.split(separator, Qt::SkipEmptyParts);
#else
    return string.split(separator, QString::SkipEmptyParts);
#endif
}

} // namespace GUIUtil

#endif // BITCOIN_QT_GUIUTIL_H
