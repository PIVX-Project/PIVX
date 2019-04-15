#ifndef PWIDGET_H
#define PWIDGET_H

#include <QObject>
#include <QWidget>
#include <QString>

class PIVXGUI;
class ClientModel;
class WalletModel;

namespace Ui {
class PWidget;
}

class PWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PWidget(PIVXGUI* _window = nullptr, QWidget *parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);

signals:
    void message(const QString& title, const QString& body, unsigned int style, bool* ret = nullptr);

protected slots:
    void changeTheme(bool isLightTheme, QString &theme);

protected:
    ClientModel* clientModel;
    WalletModel* walletModel;

    virtual void loadClientModel();
    virtual void loadWalletModel();

    void inform(const QString& message);
    void warn(const QString& title, const QString& message);
    void ask(const QString& title, const QString& message, bool* ret);
    void emitMessage(const QString& title, const QString& message, unsigned int style, bool* ret = nullptr);

private:
    PIVXGUI* window;

};

#endif // PWIDGET_H
