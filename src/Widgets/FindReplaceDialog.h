#pragma once

#include "Shared.h"

#include "FindReplaceRules.h"

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

class AbstractRundownWidget;
class RundownTreeWidget;
class RundownWidget;

// Edit > Find and Replace (build 310): text replaced across the open rundown,
// or every open rundown, in the kinds of text ticked - item labels, template
// field values, operator notes, HTML page addresses, custom AMCP commands,
// shell command lines.
//
// Find All lists every match first, with Show to go to its item. Replace All
// is one undo step per rundown (Edit > Undo in that rundown takes it back);
// a locked rundown is left alone and the window says so. A label is changed
// through the item's own label handler, so every item type redraws it as it
// does from the Inspector. Text rules: Common/FindReplaceRules.h
// (tools/test-findreplace).
class WIDGETS_EXPORT FindReplaceDialog : public QDialog
{
    Q_OBJECT

    public:
        static void open(RundownWidget* area, QWidget* parent);

        explicit FindReplaceDialog(RundownWidget* area, QWidget* parent = nullptr);

    private:
        QPointer<RundownWidget> area;

        QLineEdit* editFind = nullptr;
        QLineEdit* editReplace = nullptr;
        QCheckBox* checkMatchCase = nullptr;
        QCheckBox* checkWholeWords = nullptr;
        QList<QPair<QCheckBox*, int>> kindBoxes;
        QRadioButton* radioThis = nullptr;
        QRadioButton* radioAll = nullptr;
        QTreeWidget* list = nullptr;
        QLabel* summaryLabel = nullptr;
        QPushButton* buttonFind = nullptr;
        QPushButton* buttonReplace = nullptr;
        QPushButton* buttonShow = nullptr;

        // One piece of text that matches, and where it lives.
        struct Hit
        {
            QPointer<RundownTreeWidget> rundown;
            QTreeWidgetItem* item = nullptr;
            FindReplaceRules::Kind kind = FindReplaceRules::Label;
            QString where;      // the field's name, or which command line
            QString text;
            int matches = 0;
        };
        QList<Hit> hits;

        FindReplaceRules::Options options() const;
        int kinds() const;
        QList<RundownTreeWidget*> rundowns() const;

        void findAll();
        void replaceAll();
        void showSelected();
        void collect(RundownTreeWidget* rundown, QTreeWidgetItem* parent);
        void collectItem(RundownTreeWidget* rundown, QTreeWidgetItem* item, AbstractRundownWidget* widget);
        int replaceIn(RundownTreeWidget* rundown, QTreeWidgetItem* item, AbstractRundownWidget* widget);
};
