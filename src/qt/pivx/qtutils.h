#ifndef QTUTILS_H
#define QTUTILS_H

#include <QWidget>
#include <QDialog>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QPoint>
#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include "qt/pivx/PIVXGUI.h"


bool openDialog(QDialog *widget, PIVXGUI *gui);
void closeDialog(QDialog *widget, PIVXGUI *gui);
void openDialogFullScreen(QWidget *parent, QWidget * dialog);
bool openDialogWithOpaqueBackgroundY(QDialog *widget, PIVXGUI *gui, double posX = 3, int posY = 5);
bool openDialogWithOpaqueBackground(QDialog *widget, PIVXGUI *gui, double posX = 3);
bool openDialogWithOpaqueBackgroundFullScreen(QDialog *widget, PIVXGUI *gui);

void openSnackbar(QWidget *parent, PIVXGUI *gui, QString text){
    return;
}


// Helpers
void updateStyle(QWidget* widget);

// Settings
QSettings* getSettings();
void setupSettings(QSettings *settings);

// Theme
QString getLightTheme();
QString getDarkTheme();

bool isLightTheme();
void setTheme(bool isLight);

#endif // QTUTILS_H
