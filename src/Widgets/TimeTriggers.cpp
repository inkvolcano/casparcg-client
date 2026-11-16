#include "TimeTriggers.h"

#include "EventManager.h"
#include "Global.h"
#include "TimeTriggerRules.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Rundown/RundownTreeWidget.h"
#include "Rundown/RundownWidgetHelper.h"

#include "Commands/AbstractCommand.h"
#include "Events/StatusbarEvent.h"
#include "Models/LibraryModel.h"

#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidgetItem>

namespace
{
    const char* const BADGE_NAME = "labelFireAt";
}

TimeTriggers::TimeTriggers(QWidget* rundownArea, QObject* parent)
    : QObject(parent), area(rundownArea)
{
    this->timer.setTimerType(Qt::PreciseTimer);
    this->timer.setInterval(1000);
    QObject::connect(&this->timer, &QTimer::timeout, this, [this]() { tick(); });

    // On the second, so a row's countdown and its firing agree with the clock.
    QTimer::singleShot(1000 - QTime::currentTime().msec(), this, [this]() {
        this->lastLook = QDateTime::currentDateTime();
        this->timer.start();
        tick();
    });
}

void TimeTriggers::setArmed(bool on)
{
    if (this->armed == on)
        return;

    this->armed = on;
    this->lastLook = QDateTime::currentDateTime();
    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(on
        ? QString("Time triggers ARMED: items with a Fire at time play when it comes")
        : QString("Time triggers disarmed: nothing fires by the clock"), 6000, on));
    tick();
}

void TimeTriggers::tick()
{
    if (this->area.isNull())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime previous = this->lastLook;
    this->lastLook = now;

    for (RundownTreeWidget* rundown : this->area->findChildren<RundownTreeWidget*>())
    {
        RundownTreeBaseWidget* tree = rundown->treeWidget();
        if (tree == nullptr)
            continue;

        QList<QTreeWidgetItem*> pending;
        for (int i = 0; i < tree->topLevelItemCount(); i++)
            pending << tree->topLevelItem(i);

        while (!pending.isEmpty())
        {
            QTreeWidgetItem* item = pending.takeFirst();
            for (int c = 0; c < item->childCount(); c++)
                pending << item->child(c);

            QWidget* row = tree->itemWidget(item, 0);
            AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(row);
            if (rundownItem == nullptr || rundownItem->getCommand() == nullptr)
                continue;

            AbstractCommand* command = rundownItem->getCommand();
            const QTime at = TimeTriggerRules::parse(command->getFireAt());

            // Most rows have no time: they are passed without looking inside them,
            // once a second, unless they once had a badge to put away.
            if (!at.isValid())
            {
                if (row->property(BADGE_NAME).toBool())
                {
                    if (QLabel* old = row->findChild<QLabel*>(BADGE_NAME))
                        old->hide();
                    row->setProperty(BADGE_NAME, false);
                }
                continue;
            }

            QLabel* badge = row->findChild<QLabel*>(BADGE_NAME);

            const bool disabled = command->getDisabled();
            if (badge == nullptr)
            {
                QFrame* host = row->findChild<QFrame*>("frameItem");
                if (host == nullptr)
                    continue;
                badge = new QLabel(host);
                badge->setObjectName(BADGE_NAME);
                row->setProperty(BADGE_NAME, true);
                badge->move(RundownWidgetHelper::BADGE_WIDTH + 128, 18);
            }

            QString text = QString::fromUtf8("\xe2\x8f\xb0 ") + at.toString("HH:mm:ss") + "  " + TimeTriggerRules::countdown(at, now);
            if (disabled)
                text += "  (disabled)";
            else if (!this->armed)
                text += "  (not armed)";
            badge->setText(text);
            badge->setStyleSheet(this->armed && !disabled
                ? "background-color: rgba(200, 140, 30, 220); color: white; font-size: 9px; font-weight: bold; border-radius: 3px; padding: 0px 4px;"
                : "background-color: rgba(60, 60, 60, 200); color: rgb(190, 190, 190); font-size: 9px; border-radius: 3px; padding: 0px 4px;");
            badge->setToolTip(this->armed
                ? QString("Fires at %1 every day while time triggers are armed (Rundown > Arm Time Triggers).").arg(at.toString("HH:mm:ss"))
                : QString("Fires at %1 every day once time triggers are armed (Rundown > Arm Time Triggers). They are off now.").arg(at.toString("HH:mm:ss")));
            row->setProperty(BADGE_NAME, true);
            badge->adjustSize();
            badge->show();
            badge->raise();

            if (this->armed && !disabled && TimeTriggerRules::due(at, previous, now))
            {
                LibraryModel* model = rundownItem->getLibraryModel();
                const QString label = model == nullptr ? QString() : (model->getLabel().isEmpty() ? model->getName() : model->getLabel());
                qInfo("Time trigger: %s at %s", qPrintable(label), qPrintable(at.toString("HH:mm:ss")));

                // As if Play were pressed on it - never redirected to preview by a
                // key that happens to be held at that moment.
                rundown->executeCommand(Playout::PlayoutType::Play, Action::ActionType::KeyPress, item, false);
                EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(
                    QString("Time trigger: '%1' fired at %2").arg(label, at.toString("HH:mm:ss")), 6000));
            }
        }
    }
}
