#include "LiveRunSheet.h"

#include "DatabaseManager.h"
#include "LiveRunSheetPage.h"
#include "LiveRunSheetRules.h"
#include "RundownTiming.h"
#include "RunSheetRules.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTimingBar.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Rundown/RundownTreeWidget.h"
#include "Rundown/RundownWidget.h"

#include "Commands/AbstractCommand.h"
#include "Models/LibraryModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <QtWidgets/QTreeWidgetItem>

LiveRunSheet& LiveRunSheet::getInstance()
{
    static LiveRunSheet* instance = new LiveRunSheet();
    return *instance;
}

bool LiveRunSheet::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("LiveRunSheet").getValue() == "true";
}

void LiveRunSheet::setEnabled(bool on)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "LiveRunSheet", on ? "true" : "false"));
}

void LiveRunSheet::setArea(QWidget* rundownArea)
{
    this->area = rundownArea;
}

void LiveRunSheet::forget(const AbstractCommand* command)
{
    this->onAir.remove(command);
}

void LiveRunSheet::noteExecuted(AbstractRundownWidget* widget, Playout::PlayoutType type, int channel)
{
    if (widget == nullptr || widget->getCommand() == nullptr)
        return;

    AbstractCommand* command = widget->getCommand();
    const QString device = widget->getLibraryModel() != nullptr ? widget->getLibraryModel()->getDeviceName() : QString();

    switch (type)
    {
        case Playout::PlayoutType::Play:
        case Playout::PlayoutType::PlayNow:
        {
            if (!this->onAir.contains(command))
                QObject::connect(command, &QObject::destroyed, this, [this, command]() { forget(command); });
            OnAir entry;
            entry.firedAt = QDateTime::currentMSecsSinceEpoch();
            entry.device = device;
            entry.channel = channel;
            entry.layer = command->getVideolayer();
            this->onAir.insert(command, entry);
            break;
        }
        case Playout::PlayoutType::Stop:
        case Playout::PlayoutType::Clear:
            this->onAir.remove(command);
            break;
        case Playout::PlayoutType::ClearVideoLayer:
        case Playout::PlayoutType::ClearChannel:
        {
            // Everything on that channel (or that layer of it) is off air.
            const bool wholeChannel = type == Playout::PlayoutType::ClearChannel;
            for (auto it = this->onAir.begin(); it != this->onAir.end();)
            {
                if (it.value().device == device && it.value().channel == channel
                    && (wholeChannel || it.value().layer == command->getVideolayer()))
                    it = this->onAir.erase(it);
                else
                    ++it;
            }
            break;
        }
        default:
            break;
    }
}

QByteArray LiveRunSheet::page()
{
    return QByteArray(LiveRunSheetPage::HTML);
}

QByteArray LiveRunSheet::snapshot()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QJsonObject root;
    root.insert("now", double(now));
    root.insert("clock", QDateTime::fromMSecsSinceEpoch(now).toString("HH:mm:ss"));

    QJsonArray rundowns;
    if (!this->area.isNull())
    {
        RundownWidget* rundownArea = qobject_cast<RundownWidget*>(this->area.data());
        RundownTreeWidget* onScreen = rundownArea != nullptr ? rundownArea->activeTreeWidget() : nullptr;

        // The one on screen first, then the rest in the order they were found.
        QList<RundownTreeWidget*> trees = this->area->findChildren<RundownTreeWidget*>();
        if (onScreen != nullptr && trees.removeOne(onScreen))
            trees.prepend(onScreen);

        for (RundownTreeWidget* rundown : trees)
        {
            RundownTreeBaseWidget* tree = rundown->treeWidget();
            if (tree == nullptr)
                continue;

            RundownTimingBar* bar = rundown->getTimingBar();
            QHash<QTreeWidgetItem*, double> timed;
            if (bar != nullptr)
            {
                for (const QPair<QTreeWidgetItem*, double>& entry : bar->timedItems())
                    timed.insert(entry.first, entry.second);
            }

            struct Row
            {
                QJsonObject json;
                double length = -1;
            };
            QList<Row> rows;
            QList<LiveRunSheetRules::Pick> picks;
            int selected = -1;
            QTreeWidgetItem* current = tree->currentItem();

            QList<QPair<QTreeWidgetItem*, int>> stack;
            for (int i = tree->topLevelItemCount() - 1; i >= 0; i--)
                stack << qMakePair(tree->topLevelItem(i), 0);
            while (!stack.isEmpty())
            {
                const QPair<QTreeWidgetItem*, int> next = stack.takeLast();
                for (int c = next.first->childCount() - 1; c >= 0; c--)
                    stack << qMakePair(next.first->child(c), next.second + 1);

                AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(next.first, 0));
                if (widget == nullptr || widget->getCommand() == nullptr || widget->getLibraryModel() == nullptr)
                    continue;

                AbstractCommand* command = widget->getCommand();
                LibraryModel* model = widget->getLibraryModel();
                const double length = timed.value(next.first, -1);

                double elapsed = 0, total = 0;
                bool paused = false;
                const bool clipKnown = widget->clipTime(&elapsed, &total, &paused);
                const qint64 firedAt = this->onAir.contains(command) ? this->onAir.value(command).firedAt : -1;
                const LiveRunSheetRules::Live live = LiveRunSheetRules::liveState(clipKnown, elapsed, total, paused, firedAt,
                                                                                   clipKnown ? -1 : length, now);

                QJsonObject json;
                json.insert("depth", next.second);
                json.insert("label", model->getLabel().isEmpty() ? model->getName() : model->getLabel());
                json.insert("type", model->getType());
                if (!model->getDeviceName().isEmpty() && command->getChannel() > 0)
                    json.insert("where", QString("%1 %2-%3").arg(model->getDeviceName()).arg(command->getChannel()).arg(command->getVideolayer()));
                json.insert("length", length >= 0 ? RundownTiming::formatLength(length) : QString());
                if (!command->getNote().isEmpty())
                    json.insert("note", command->getNote());
                if (command->getDisabled())
                    json.insert("disabled", true);
                json.insert("state", LiveRunSheetRules::stateText(live.state));
                if (live.remaining >= 0)
                {
                    json.insert("remaining", live.remaining);
                    if (live.state != LiveRunSheetRules::State::Paused)
                        json.insert("endsAt", double(now) + live.remaining * 1000.0);
                }

                LiveRunSheetRules::Pick pickRow;
                pickRow.onAir = live.state != LiveRunSheetRules::State::Idle;
                pickRow.disabled = command->getDisabled();
                pickRow.remaining = live.remaining;
                pickRow.firedAt = firedAt;
                if (next.first == current)
                    selected = rows.count();

                Row row;
                row.json = json;
                row.length = length;
                rows << row;
                picks << pickRow;
            }

            QList<double> lengths;
            for (const Row& row : rows)
                lengths << row.length;
            const QString hardOut = bar != nullptr ? bar->hardOut() : QString();
            const QStringList starts = RunSheetRules::starts(lengths, RundownTiming::parseHardOut(hardOut));

            QJsonArray items;
            for (int i = 0; i < rows.count(); i++)
            {
                QJsonObject json = rows.at(i).json;
                json.insert("n", i + 1);
                if (!starts.value(i).isEmpty())
                    json.insert("start", starts.value(i));
                items.append(json);
            }

            QJsonObject entry;
            entry.insert("name", rundown->getActiveRundown() == Rundown::DEFAULT_NAME
                ? QString("Untitled rundown") : QFileInfo(rundown->getActiveRundown()).completeBaseName());
            entry.insert("onScreen", rundown == onScreen);
            if (!hardOut.isEmpty())
                entry.insert("hardOut", hardOut);
            entry.insert("total", RundownTiming::formatLength(RunSheetRules::total(lengths)));

            // The presenter view's two items, chosen here so the rules are tested.
            const int primary = LiveRunSheetRules::primaryOnAir(picks);
            const int upNext = LiveRunSheetRules::nextItem(picks, selected);
            if (primary >= 0)
                entry.insert("primary", primary + 1);
            if (upNext >= 0)
                entry.insert("next", upNext + 1);
            entry.insert("items", items);
            rundowns.append(entry);
        }
    }
    root.insert("rundowns", rundowns);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
