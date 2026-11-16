#pragma once

#include "Shared.h"

#include "ReadinessRules.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTreeWidget;
class QWidget;
QT_END_NAMESPACE

class RundownTreeBaseWidget;

// Rundown > Check Before Going Live: everything that would stop the open
// rundown from going to air as meant, in one list, each with Show to select
// the item it is about.
//
//   - media and templates that are not there (the same check as the row
//     warnings, MissingMediaScanner);
//   - servers that are not connected, or not set up here;
//   - items aimed at a locked channel, which will not play;
//   - sheet sources that do not answer - read once each, with no forced read,
//     so a held copy counts and the shared API budget is spared.
//
// Run when someone asks for it; nothing is watched in the background. The
// server and lock rules are in Common/ReadinessRules.h (tools/test-readiness).
class WIDGETS_EXPORT ReadinessDialog : public QDialog
{
    Q_OBJECT

    public:
        static void openFor(RundownTreeBaseWidget* tree, QWidget* parent);

        explicit ReadinessDialog(RundownTreeBaseWidget* tree, QWidget* parent = nullptr);

    private:
        QPointer<RundownTreeBaseWidget> tree;

        QLabel* summaryLabel = nullptr;
        QTreeWidget* list = nullptr;
        QPushButton* buttonShow = nullptr;
        QPushButton* buttonCheck = nullptr;

        // The rundown's rows, by the id the findings carry.
        QList<QPointer<QWidget>> rows;
        QList<ReadinessRules::Item> items;
        QList<ReadinessRules::Finding> findings;
        QString notChecked;

        // A sheet read in flight: which items it is for, and what to call it.
        struct PendingSheet
        {
            QList<int> itemIds;
            QString what;
        };
        QMap<QString, PendingSheet> pendingSheets;
        QTimer sheetTimeout;

        void runChecks();
        void checkSheets();
        void sheetAnswered(const QString& requestId, const QString& failure);
        void refresh();
        void showSelected();
};
