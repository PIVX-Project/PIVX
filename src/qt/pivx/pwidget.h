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
    void showHide(bool show);
    bool execDialog(QDialog *dialog, int xDiv = 3, int yDiv = 5);

protected slots:
    void changeTheme(bool isLightTheme, QString &theme);

protected:
    PIVXGUI* window = nullptr;
    ClientModel* clientModel = nullptr;
    WalletModel* walletModel = nullptr;

    virtual void loadClientModel();
    virtual void loadWalletModel();

    void showHideOp(bool show);
    void inform(const QString& message);
    void warn(const QString& title, const QString& message);
    void ask(const QString& title, const QString& message, bool* ret);
    void emitMessage(const QString& title, const QString& message, unsigned int style, bool* ret = nullptr);


};

#endif // PWIDGET_H
