#ifndef TOOLTIPMENU_H
#define TOOLTIPMENU_H

#include <QWidget>

namespace Ui {
class TooltipMenu;
}

class TooltipMenu : public QWidget
{
    Q_OBJECT

public:
    explicit TooltipMenu(QWidget *parent = nullptr);
    ~TooltipMenu() override;

    virtual void showEvent(QShowEvent *event) override;
private:
    Ui::TooltipMenu *ui;
};

#endif // TOOLTIPMENU_H
