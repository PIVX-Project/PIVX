#ifndef CONTACTDROPDOWNROW_H
#define CONTACTDROPDOWNROW_H

#include <QWidget>

namespace Ui {
class ContactDropdownRow;
}

class ContactDropdownRow : public QWidget
{
    Q_OBJECT

public:
    explicit ContactDropdownRow(bool isLightTheme, bool isHover, QWidget *parent = nullptr);
    ~ContactDropdownRow();

    void update(bool isLightTheme, bool isHover, bool isSelected);
protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

private:
    Ui::ContactDropdownRow *ui;
};

#endif // CONTACTDROPDOWNROW_H
