#ifndef PWIDGET_H
#define PWIDGET_H

#include <QObject>
#include <QWidget>

class PIVXGUI;

namespace Ui {
class PWidget;
}

class PWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PWidget(PIVXGUI* _window = nullptr, QWidget *parent = nullptr);

signals:
    void message(QString &message);

protected slots:
    void changeTheme(bool isLightTheme, QString &theme);

private:
    PIVXGUI* window;

};

#endif // PWIDGET_H
