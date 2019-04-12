#ifndef TOOLTIPMENU_H
#define TOOLTIPMENU_H

#include <QWidget>

class PIVXGUI;

namespace Ui {
class TooltipMenu;
}

class TooltipMenu : public QWidget
{
    Q_OBJECT

public:
    explicit TooltipMenu(PIVXGUI* _window, QWidget *parent = nullptr);
    ~TooltipMenu() override;

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
};

#endif // TOOLTIPMENU_H
