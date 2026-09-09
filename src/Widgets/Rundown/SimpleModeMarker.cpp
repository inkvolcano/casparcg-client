#include "SimpleModeMarker.h"

#include "AbstractRundownWidget.h"
#include "RundownWidgetHelper.h"

#include "Commands/AbstractCommand.h"

#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>

namespace
{
    const char* const MARKER_NAME = "labelSimpleModeKey";

    // To the right of the missing-media mark, which sits at BADGE_WIDTH + 2 and is
    // sixteen wide. Same row, so a row carrying both reads as two small marks
    // rather than as one obscuring the other.
    const int MARKER_X = RundownWidgetHelper::BADGE_WIDTH + 20;
    const int MARKER_Y = 18;
}

SimpleModeMarker::SimpleModeMarker(QObject* parent)
    : QObject(parent)
{
}

void SimpleModeMarker::sweep(QTreeWidget* tree)
{
    if (tree == nullptr)
        return;

    QTreeWidgetItem* root = tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        walk(tree, root->child(i));
}

void SimpleModeMarker::walk(QTreeWidget* tree, QTreeWidgetItem* item)
{
    QWidget* widget = tree->itemWidget(item, 0);
    AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);

    if (rundownItem != nullptr && rundownItem->getCommand() != nullptr)
    {
        AbstractCommand* command = rundownItem->getCommand();
        apply(widget, command->getShowInSimpleMode());

        // The flag is ticked in the Inspector with this row on screen, so a sweep
        // on its own would be right only until the next tick. UniqueConnection
        // makes re-sweeping free rather than doubling the connections.
        QObject::connect(command, &AbstractCommand::showInSimpleModeChanged,
                         this, [this, widget](bool onGrid) { apply(widget, onGrid); },
                         Qt::UniqueConnection);
    }

    for (int i = 0; i < item->childCount(); i++)
        walk(tree, item->child(i));
}

void SimpleModeMarker::apply(QWidget* widget, bool onGrid)
{
    if (widget == nullptr)
        return;

    QLabel* marker = widget->findChild<QLabel*>(MARKER_NAME);

    if (!onGrid)
    {
        // Kept and hidden rather than deleted: a row toggled on and off repeatedly
        // would otherwise churn a widget every time.
        if (marker != nullptr)
            marker->hide();

        return;
    }

    if (marker == nullptr)
    {
        QWidget* host = widget->findChild<QFrame*>("frameItem");
        if (host == nullptr)
            return;

        marker = new QLabel(host);
        marker->setObjectName(MARKER_NAME);
        marker->setText(QString::fromUtf8("\xe2\x96\xa3"));   // a key: filled square in a square
        marker->setFixedSize(14, 14);
        marker->setAlignment(Qt::AlignCenter);
        marker->setToolTip("This item has a button on the Simple Mode grid");
        marker->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        marker->setStyleSheet("color: rgb(150, 200, 255); background-color: rgba(0, 0, 0, 90);"
                              "border-radius: 3px; font-size: 11px; font-weight: bold;");
        marker->move(MARKER_X, MARKER_Y);
    }

    marker->show();
    marker->raise();
}
