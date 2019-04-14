#ifndef QTUTILS_H
#define QTUTILS_H

#include <QWidget>
#include <QDialog>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QPoint>
#include <QString>
#include <QColor>
#include <QSettings>
#include <QPixmap>
#include <QStandardPaths>
#include "qt/pivx/PIVXGUI.h"

static Qt::Modifier SHORT_KEY
#ifdef Q_OS_MAC
        = Qt::CTRL;
#else
        = Qt::ALT;
#endif

bool openDialog(QDialog *widget, PIVXGUI *gui);
void closeDialog(QDialog *widget, PIVXGUI *gui);
void openDialogFullScreen(QWidget *parent, QWidget * dialog);
bool openDialogWithOpaqueBackgroundY(QDialog *widget, PIVXGUI *gui, double posX = 3, int posY = 5);
bool openDialogWithOpaqueBackground(QDialog *widget, PIVXGUI *gui, double posX = 3);
bool openDialogWithOpaqueBackgroundFullScreen(QDialog *widget, PIVXGUI *gui);

void openSnackbar(QWidget *parent, PIVXGUI *gui, QString text){
    // TODO:Complete me..
    return;
}

//
QPixmap encodeToQr(QString str, QString &errorStr, QColor qrColor = Qt::black);

// Helpers
void updateStyle(QWidget* widget);
QColor getRowColor(bool isLightTheme, bool isHovered, bool isSelected);

// Settings
QSettings* getSettings();
void setupSettings(QSettings *settings);

// Theme
QString getLightTheme();
QString getDarkTheme();

bool isLightTheme();
void setTheme(bool isLight);

void setCssEditLine(QLineEdit *edit, bool isValid, bool forceUpdate = false);
void setCssEditLineDialog(QLineEdit *edit, bool isValid, bool forceUpdate = false);

void setCssBtnPrimary(QPushButton *btn, bool forceUpdate = false);
void forceUpdateStyle(QWidget *widget, bool forceUpdate);

#endif // QTUTILS_H
