#include "WatchFolders.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "WatchFolderRules.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Rundown/RundownTreeWidget.h"

#include "CasparDevice.h"
#include "Commands/GroupCommand.h"
#include "Events/StatusbarEvent.h"
#include "Models/CasparMedia.h"
#include "Models/DeviceModel.h"
#include "Models/LibraryModel.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <QtWidgets/QTreeWidgetItem>

#include <functional>

WatchFolders& WatchFolders::getInstance()
{
    static WatchFolders* instance = new WatchFolders();
    return *instance;
}

WatchFolders::WatchFolders()
    : QObject()
{
    this->timer = new QTimer(this);
    QObject::connect(this->timer, &QTimer::timeout, this, [this]() {
        poll();
        this->timer->setInterval(intervalSeconds() * 1000);    // a changed setting takes over here
    });
}

void WatchFolders::setArea(QWidget* rundownArea)
{
    this->area = rundownArea;
    this->timer->start(intervalSeconds() * 1000);
}

int WatchFolders::intervalSeconds()
{
    const int seconds = DatabaseManager::getInstance().getConfigurationByName("WatchFolderSeconds").getValue().toInt();
    return seconds >= 2 && seconds <= 3600 ? seconds : 15;
}

QStringList WatchFolders::libraryNames(const QString& server)
{
    QStringList names;
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(server);
    if (model == nullptr)
        return names;
    for (const LibraryModel& media : DatabaseManager::getInstance().getLibraryMediaByDeviceAddress(model->getAddress()))
        names.append(media.getName());
    return names;
}

QList<WatchFolders::Watch> WatchFolders::watches() const
{
    QList<Watch> found;
    if (this->area.isNull())
        return found;

    const QList<RundownTreeWidget*> rundowns = this->area->findChildren<RundownTreeWidget*>();
    for (RundownTreeWidget* rundown : rundowns)
    {
        RundownTreeBaseWidget* tree = rundown->treeWidget();
        if (tree == nullptr)
            continue;

        std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* parent) {
            for (int i = 0; i < parent->childCount(); i++)
            {
                QTreeWidgetItem* item = parent->child(i);
                AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(item, 0));
                GroupCommand* group = widget != nullptr ? dynamic_cast<GroupCommand*>(widget->getCommand()) : nullptr;
                if (group != nullptr && group->getWatchEnabled() && !group->getWatchServer().isEmpty()
                    && !WatchFolderRules::normaliseFolder(group->getWatchFolder()).isEmpty())
                {
                    Watch watch;
                    watch.rundown = rundown;
                    watch.item = item;
                    watch.group = group;
                    found.append(watch);
                }
                if (item->childCount() > 0)
                    walk(item);
            }
        };
        walk(tree->invisibleRootItem());
    }
    return found;
}

void WatchFolders::poll()
{
    QSet<QString> servers;
    for (const Watch& watch : watches())
        servers.insert(watch.group->getWatchServer());

    for (const QString& server : servers)
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(server);
        if (device == nullptr || !device->isConnected())
            continue;

        // Any listing from this server is looked at, whoever asked for it.
        QObject::connect(device.data(), &CasparDevice::mediaChanged, this, &WatchFolders::mediaArrived, Qt::UniqueConnection);
        device->refreshMedia();
    }
}

void WatchFolders::mediaArrived(const QList<CasparMedia>& media, CasparDevice& device)
{
    QString server;
    for (const DeviceModel& model : DeviceManager::getInstance().getDeviceModels())
    {
        if (DeviceManager::getInstance().getDeviceByName(model.getName()).data() == &device)
            server = model.getName();
    }
    if (server.isEmpty())
        return;

    // After the Library has taken the same listing in, so a new item finds its
    // Library row (thumbnail, details) and the missing-media sweep sees the truth.
    const QList<CasparMedia> listing = media;
    QTimer::singleShot(0, this, [this, listing, server]() { apply(listing, server); });
}

void WatchFolders::apply(const QList<CasparMedia>& media, const QString& server)
{
    QList<WatchFolderRules::Media> all;
    all.reserve(media.count());
    for (const CasparMedia& item : media)
    {
        WatchFolderRules::Media m;
        m.name = item.getName();
        m.type = item.getType();
        m.timecode = item.getTimecode();
        all.append(m);
    }

    QSet<RundownTreeWidget*> touched;
    QSet<RundownTreeWidget*> missingChanged;
    for (const Watch& watch : watches())
    {
        if (watch.rundown.isNull() || watch.group->getWatchServer() != server)
            continue;
        if (watch.rundown->isLocked())
            continue;

        RundownTreeBaseWidget* tree = watch.rundown->treeWidget();
        const QString folder = watch.group->getWatchFolder();
        QStringList seen = watch.group->getWatchSeen();

        const QList<WatchFolderRules::Media> fresh = WatchFolderRules::fresh(all, folder, seen);
        int added = 0;
        for (const WatchFolderRules::Media& m : fresh)
        {
            const LibraryModel model(0, m.name, m.name, server, m.type, 0, m.timecode);
            if (watch.rundown->appendToGroup(watch.item, model, watch.group->getWatchChannel(), watch.group->getWatchLayer()))
                added++;
            seen.append(m.name.toUpper());
        }
        if (!fresh.isEmpty())
        {
            watch.group->setWatchSeen(seen);
            touched.insert(watch.rundown.data());
            AbstractRundownWidget* groupWidget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(watch.item, 0));
            const QString label = groupWidget != nullptr && groupWidget->getLibraryModel() != nullptr
                ? groupWidget->getLibraryModel()->getLabel() : QString("a group");
            qInfo("Watch folder %s on %s: %d new item(s) in %s", qPrintable(WatchFolderRules::normaliseFolder(folder)),
                  qPrintable(server), added, qPrintable(label));
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(
                QString("Watch folder %1: %2 new item%3 in %4").arg(WatchFolderRules::normaliseFolder(folder)).arg(added)
                    .arg(added == 1 ? "" : "s").arg(label), 5000));
        }

        // A file that left the folder: its item stays, and is marked missing.
        QStringList itemNames;
        for (int i = 0; i < watch.item->childCount(); i++)
        {
            AbstractRundownWidget* child = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(watch.item->child(i), 0));
            if (child != nullptr && child->getLibraryModel() != nullptr)
                itemNames.append(child->getLibraryModel()->getName());
        }
        // Swept only when what is gone changes - a file staying gone is not
        // a reason to sweep the rundown on every look, and one coming back is.
        const QStringList goneNow = WatchFolderRules::gone(all, folder, itemNames);
        if (goneNow != this->goneBefore.value(watch.group))
        {
            this->goneBefore.insert(watch.group, goneNow);
            missingChanged.insert(watch.rundown.data());
        }
    }

    for (RundownTreeWidget* rundown : touched)
        rundown->finishAppending();
    for (RundownTreeWidget* rundown : missingChanged)
        rundown->checkMissingMedia();
}
