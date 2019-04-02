#ifndef SEND_H
#define SEND_H

#include <QWidget>
#include <QPushButton>

#include "qt/pivx/contactsdropdown.h"
#include "qt/pivx/sendmultirow.h"
#include "walletmodel.h"

static const int MAX_SEND_POPUP_ENTRIES = 8;

class PIVXGUI;
class ClientModel;
class WalletModel;

namespace Ui {
class send;
class QPushButton;
}

class SendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SendWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SendWidget();

    void addEntry();

    void setClientModel(ClientModel* clientModel);
    void setModel(WalletModel* model);

signals:
    // Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style);

public slots:
    void onChangeAddressClicked();
    void onChangeCustomFeeClicked();
    void onCoinControlClicked();

protected:
    void resizeEvent(QResizeEvent *event);

private slots:
    void onPIVSelected();
    void onzPIVSelected();
    void onSendClicked();
    void changeTheme(bool isLightTheme, QString& theme);
    void onContactsClicked();
    void onAddEntryClicked();
    void clearEntries();
private:
    Ui::send *ui;
    PIVXGUI* window;
    QPushButton *coinIcon;
    QPushButton *btnContacts;

    ClientModel* clientModel;
    WalletModel* walletModel;

    QList<SendMultiRow*> entries;

    ContactsDropdown *menuContacts = nullptr;
    SendMultiRow *sendMultiRow;

    bool isPIV = true;
    void resizeMenu();
    void send(QList<SendCoinsRecipient> recipients);

    // Process WalletModel::SendCoinsReturn and generate a pair consisting
    // of a message and message flags for use in emit message().
    // Additional parameter msgArg can be used via .arg(msgArg).
    void processSendCoinsReturn(const WalletModel::SendCoinsReturn& sendCoinsReturn, const QString& msgArg = QString(), bool fPrepare = false);
};

#endif // SEND_H
