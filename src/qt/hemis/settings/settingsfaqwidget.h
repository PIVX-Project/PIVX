// Copyright (c) 2019-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSFAQWIDGET_H
#define SETTINGSFAQWIDGET_H

#include <QDialog>

class GMModel;
class hemisGUI;

namespace Ui {
class SettingsFaqWidget;
}

class SettingsFaqWidget : public QDialog
{
    Q_OBJECT
public:
    enum Section {
        INTRO,
        UNSPENDABLE,
        STAKE,
        SUPPORT,
        GAMEMASTER,
        GMCONTROLLER
    };

    explicit SettingsFaqWidget(hemisGUI* parent, GMModel* _model);
    ~SettingsFaqWidget();

    void showEvent(QShowEvent *event) override;

public Q_SLOTS:
   void windowResizeEvent(QResizeEvent* event);
   void setSection(Section _section);
private Q_SLOTS:
    void onFaqClicked(const QWidget* const widget);
private:
    Ui::SettingsFaqWidget *ui;
    Section section = INTRO;

    // This needs to be edited if changes are made to the Section enum.
    std::vector<QPushButton*> getButtons();

    // Formats a QString into a FAQ Content section with HTML
    static inline QString formatFAQContent(const QString& str) {
        return "<html><head/><body>" + str + "</body></html>";
    }

    // Formats a QString into a FAQ content paragraph with HTML
    static inline QString formatFAQParagraph(const QString& str) {
        return "<p align=\"justify\">" + str + "</p>";
    }

    // Formats a QString into a FAQ content ordered list with HTML
    static inline QString formatFAQOrderedList(const QString& str) {
        return "<ol>" + str + "</ol>";
    }

    // Formats a QString into a FAQ content un-ordered list with HTML
    static inline QString formatFAQUnorderedList(const QString& str) {
        return "<ul>" + str + "</ul>";
    }

    // Formats a QString into a FAQ content list item with HTML
    static inline QString formatFAQListItem(const QString& str) {
        return "<li>" + str + "</li>";
    }
};

#endif // SETTINGSFAQWIDGET_H
