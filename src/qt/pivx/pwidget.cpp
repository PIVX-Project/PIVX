#include "qt/pivx/pwidget.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/moc_pwidget.cpp"

PWidget::PWidget(PIVXGUI* _window, QWidget *parent) : QWidget((parent) ? parent : _window), window(_window){init();}
PWidget::PWidget(PWidget* parent) : QWidget(parent), window(parent->getWindow()){init();}

void PWidget::init() {
    if(window)
        connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(onChangeTheme(bool, QString&)));
}

void PWidget::setClientModel(ClientModel* model){
    this->clientModel = model;
    loadClientModel();
}

void PWidget::setWalletModel(WalletModel* model){
    this->walletModel = model;
    loadWalletModel();
}

void PWidget::onChangeTheme(bool isLightTheme, QString& theme){
    this->setStyleSheet(theme);
    changeTheme(isLightTheme, theme);
    updateStyle(this);
}

void PWidget::showHideOp(bool show){
    emit showHide(show);
}

void PWidget::inform(const QString& message){
    emitMessage("", message, CClientUIInterface::MSG_INFORMATION_SNACK);
}

void PWidget::warn(const QString& title, const QString& message){
    emitMessage(title, message, CClientUIInterface::MSG_ERROR);
}

bool PWidget::ask(const QString& title, const QString& message){
    bool ret = false;
    emitMessage(title, message, CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL, &ret);
    return ret;
}

void PWidget::showDialog(QDialog *dlg, int xDiv, int yDiv){
    emit execDialog(dlg, xDiv, yDiv);
}

void PWidget::emitMessage(const QString& title, const QString& body, unsigned int style, bool* ret){
    emit message(title, body, style, ret);
}

bool PWidget::verifyWalletUnlocked(){
    if (!walletModel->isWalletUnlocked()) {
        inform(tr("Wallet locked, you need to unlock it to perform this action"));
        return false;
    }
    return true;
}

void PWidget::loadClientModel(){
    // override
}

void PWidget::loadWalletModel(){
    // override
}

void PWidget::changeTheme(bool isLightTheme, QString& theme){
    // override
}