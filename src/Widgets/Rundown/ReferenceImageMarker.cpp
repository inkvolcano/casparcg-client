#include "ReferenceImageMarker.h"

#include "AbstractRundownWidget.h"
#include "RundownWidgetHelper.h"
#include "ReferenceImage.h"

#include "EventManager.h"
#include "Commands/AbstractCommand.h"

#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>

namespace
{
    const char* const MARKER_NAME = "labelReferenceImage";
    const char* const APPLIED = "referenceImageKey";

    // Beside the Simple Mode mark (BADGE_WIDTH + 20) and the missing-media mark
    // (BADGE_WIDTH + 2), on the same line, so the three read as a row of marks.
    const int MARKER_X = RundownWidgetHelper::BADGE_WIDTH + 38;
    const int MARKER_Y = 18;
}

ReferenceImageMarker::ReferenceImageMarker(QTreeWidget* tree, QObject* parent)
    : QObject(parent),
      tree(tree)
{
    this->sweepTimer.setSingleShot(true);
    this->sweepTimer.setInterval(50);
    QObject::connect(&this->sweepTimer, &QTimer::timeout, this, &ReferenceImageMarker::sweep);

    // An undo or a delete rebuilds rows; a selection is how a newly added item is
    // met for the first time, so its picture changes are heard from then on.
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownStructureChanged()), this, SLOT(queueSweep()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(queueSweep()));
}

void ReferenceImageMarker::queueSweep()
{
    this->sweepTimer.start();
}

void ReferenceImageMarker::sweep()
{
    if (this->tree.isNull())
        return;

    QTreeWidgetItem* root = this->tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        walk(root->child(i));
}

void ReferenceImageMarker::walk(QTreeWidgetItem* item)
{
    QWidget* widget = this->tree->itemWidget(item, 0);
    AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);

    if (rundownItem != nullptr && rundownItem->getCommand() != nullptr)
    {
        AbstractCommand* command = rundownItem->getCommand();
        apply(widget, command->getReferenceImage());

        // A member slot, so UniqueConnection does its job: a sweep never doubles it.
        QObject::connect(command, &AbstractCommand::referenceImageChanged, this,
                         &ReferenceImageMarker::queueSweep, Qt::UniqueConnection);
    }

    for (int i = 0; i < item->childCount(); i++)
        walk(item->child(i));
}

void ReferenceImageMarker::apply(QWidget* widget, const QString& dataUri)
{
    if (widget == nullptr)
        return;

    // The picture is tens of KB of text: the tooltips are set only when it changed.
    const uint key = dataUri.isEmpty() ? 0 : qHash(dataUri);
    if (widget->property(APPLIED).isValid() && widget->property(APPLIED).toUInt() == key)
        return;
    widget->setProperty(APPLIED, key);

    QWidget* host = widget->findChild<QFrame*>("frameItem");
    QLabel* marker = widget->findChild<QLabel*>(MARKER_NAME);

    if (dataUri.isEmpty())
    {
        if (marker != nullptr)
        {
            marker->hide();
            marker->setToolTip(QString());
        }
        if (host != nullptr)
            host->setToolTip(QString());
        return;
    }

    if (host == nullptr)
        return;

    if (marker == nullptr)
    {
        marker = new QLabel(host);
        marker->setObjectName(MARKER_NAME);
        marker->setText(QString::fromUtf8("\xf0\x9f\x96\xbc"));   // a framed picture
        marker->setFixedSize(16, 14);
        marker->setAlignment(Qt::AlignCenter);
        marker->setStyleSheet("background-color: rgba(0, 0, 0, 90); border-radius: 3px; font-size: 10px;");
        marker->move(MARKER_X, MARKER_Y);
    }

    // On the mark, and on the row itself, so hovering anywhere on the row that has
    // no tooltip of its own shows the picture.
    const QString html = ReferenceImage::tooltipHtml(dataUri);
    marker->setToolTip(html);
    host->setToolTip(html);

    marker->show();
    marker->raise();
}
