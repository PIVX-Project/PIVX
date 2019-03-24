#include "qt/pivx/qtutils.h"

//#include "snackbar.h"

#include <QFile>
#include <QStyle>

// Open dialog at the bottom
bool openDialog(QDialog *widget, PIVXGUI *gui){
    widget->setWindowFlags(Qt::CustomizeWindowHint);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    QPropertyAnimation* animation = new QPropertyAnimation(widget, "pos");
    animation->setDuration(300);
    animation->setStartValue(QPoint(0, gui->height()));
    animation->setEndValue(QPoint(0, gui->height() - widget->height()));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    widget->activateWindow();
    widget->raise();
    bool res = widget->exec();
    return res;
}

void closeDialog(QDialog *widget, PIVXGUI *gui){
    widget->setWindowFlags(Qt::CustomizeWindowHint);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    QPropertyAnimation* animation = new QPropertyAnimation(widget, "pos");
    animation->setDuration(300);
    animation->setStartValue(widget->pos());
    animation->setEndValue(QPoint(0, gui->height() + 100));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void openDialogFullScreen(QWidget *parent, QWidget * dialog){
    dialog->setWindowFlags(Qt::CustomizeWindowHint);
    dialog->move(0, 0);
    dialog->show();
    dialog->activateWindow();
    dialog->resize(parent->width(),parent->height());
}

bool openDialogWithOpaqueBackgroundY(QDialog *widget, PIVXGUI *gui, double posX, int posY){
    widget->setWindowFlags(Qt::CustomizeWindowHint);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    QPropertyAnimation* animation = new QPropertyAnimation(widget, "pos");
    animation->setDuration(300);
    int xPos = gui->width() / posX ;
    animation->setStartValue(QPoint(xPos, gui->height()));
    animation->setEndValue(QPoint(xPos, gui->height() / posY));//- (gui->height()) / posY  ));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    widget->activateWindow();
    bool res = widget->exec();
    gui->showHide(false);
    return res;
}

bool openDialogWithOpaqueBackground(QDialog *widget, PIVXGUI *gui, double posX){
    return openDialogWithOpaqueBackgroundY(widget, gui, posX, 5);
}

bool openDialogWithOpaqueBackgroundFullScreen(QDialog *widget, PIVXGUI *gui){
    widget->setWindowFlags(Qt::CustomizeWindowHint);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);

    widget->activateWindow();
    widget->resize(gui->width(),gui->height());

    QPropertyAnimation* animation = new QPropertyAnimation(widget, "pos");
    animation->setDuration(300);
    int xPos = 0;
    animation->setStartValue(QPoint(xPos, gui->height()));
    animation->setEndValue(QPoint(xPos, 0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    widget->activateWindow();
    bool res = widget->exec();
    gui->showHide(false);
    return res;
}


QString getLightTheme(){
    QFile qFile(":/res/css/style_light.css");
    QString styleSheet;
    if (qFile.open(QFile::ReadOnly)) {
        styleSheet = QLatin1String(qFile.readAll());
    }
    return styleSheet;
}

QString getDarkTheme(){
    /* CSS FILE*/
    QFile qFile(":/res/css/style_dark.css");
    QString styleSheet;
    if (qFile.open(QFile::ReadOnly)) {
        styleSheet = QLatin1String(qFile.readAll());
    }
    return styleSheet;
}


void setupSettings(QSettings *settings){
    if(!settings->contains("lightTheme")){
        settings->setValue("lightTheme", true);
    }
}

QSettings *settings = nullptr;

QSettings* getSettings(){
    if(!settings){
        QString m_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QString m_filename = "pivx_new_gui_config.ini" ;
        settings = new QSettings(m_path + "/"+ m_filename,QSettings::IniFormat);

        // Setup initial values if them are not there
        setupSettings(settings);
    }

    return settings;
}

bool isLightTheme(){
    return getSettings()->value("lightTheme", true).toBool();
}

void setTheme(bool isLight){
    getSettings()->setValue("lightTheme", isLight);
}


// Style

void updateStyle(QWidget* widget){
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
