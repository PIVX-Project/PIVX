#ifndef TOOLTIPMENU_H
#define TOOLTIPMENU_H

#include <QWidget>
#include <QModelIndex>

class PIVXGUI;
class WalletModel;

namespace Ui {
class TooltipMenu;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class TooltipMenu : public QWidget
{
    Q_OBJECT

public:
    explicit TooltipMenu(PIVXGUI* _window, QWidget *parent = nullptr);
    ~TooltipMenu() override;

    void setWalletModel(WalletModel *model);
    void setIndex(const QModelIndex &index);

    virtual void showEvent(QShowEvent *event) override;

signals:
    void message(const QString& title, const QString& message, unsigned int style, bool* ret = nullptr);

private slots:
    void deleteClicked();
    void copyClicked();
    void editClicked();

private:
    Ui::TooltipMenu *ui;
    PIVXGUI *window;
    WalletModel *walletModel = nullptr;
    QModelIndex index;
};

#endif // TOOLTIPMENU_H
