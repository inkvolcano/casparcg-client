#include "DeviceManager.h"
#include "DatabaseManager.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Events/DataChangedEvent.h"
#include "Events/MediaChangedEvent.h"
#include "Events/Inspector/TemplateChangedEvent.h"
#include "Models/DeviceModel.h"

#include <stdexcept>

#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QStringList>

#include <QtWidgets/QApplication>

Q_GLOBAL_STATIC(DeviceManager, deviceManager)

DeviceManager::DeviceManager()
{
    this->tickTimer = new QTimer(this);
    this->tickTimer->setInterval(1000);
    QObject::connect(this->tickTimer, &QTimer::timeout, this, &DeviceManager::timedLockTickUpdate);
}

DeviceManager& DeviceManager::getInstance()
{
    return *deviceManager();
}

void DeviceManager::initialize()
{
    QList<DeviceModel> models = DatabaseManager::getInstance().getDevice();
    foreach (const DeviceModel& model, models)
    {
        QSharedPointer<CasparDevice> device(new CasparDevice(model.getAddress(), model.getPort()));

        this->deviceModels.insert(model.getName(), model);
        this->devices.insert(model.getName(), device);

        emit deviceAdded(*device);

        device->connectDevice();
    }
}

void DeviceManager::uninitialize()
{
    foreach (const QString& key, this->devices.keys())
    {
        QSharedPointer<CasparDevice>& device = this->devices[key];
        device->disconnectDevice();
    }
}

void DeviceManager::refresh()
{
    QList<DeviceModel> models = DatabaseManager::getInstance().getDevice();

    // Disconnect old devices.
    foreach (const QString& key, this->devices.keys())
    {
        QSharedPointer<CasparDevice>& device = this->devices[key];

        bool foundDevice = false;
        foreach (DeviceModel model, models)
        {
            if (model.getAddress() == device->getAddress())
            {
                foundDevice = true;
                break;
            }
        }

        if (!foundDevice)
        {
            device->disconnectDevice();

            this->devices.remove(key);
            this->deviceModels.remove(key);

            emit deviceRemoved();
        }
    }

    // Connect new devices.
    foreach (DeviceModel model, models)
    {
        if (!this->devices.contains(model.getName()))
        {
            QSharedPointer<CasparDevice> device(new CasparDevice(model.getAddress(), model.getPort()));

            this->deviceModels.insert(model.getName(), model);
            this->devices.insert(model.getName(), device);

            emit deviceAdded(*device);

            device->connectDevice();
        }
    }
}

QList<DeviceModel> DeviceManager::getDeviceModels() const
{
    QList<DeviceModel> models;
    foreach (const DeviceModel& model, this->deviceModels)
        models.push_back(model);

    return models;
}

const QSharedPointer<DeviceModel> DeviceManager::getDeviceModelByName(const QString& name) const
{
    foreach (const DeviceModel& model, this->deviceModels)
    {
        if (model.getName() == name)
            return QSharedPointer<DeviceModel>(new DeviceModel(model));
    }

    qWarning("No DeviceModel found for name: %s", qPrintable(name));

    return QSharedPointer<DeviceModel>();
}

const QSharedPointer<DeviceModel> DeviceManager::getDeviceModelByAddress(const QString& address) const
{
    foreach (const DeviceModel& model, this->deviceModels)
    {
        if (model.getAddress() == address)
           return QSharedPointer<DeviceModel>(new DeviceModel(model));
    }

    qWarning("No DeviceModel found for address: %s", qPrintable(address));

    return QSharedPointer<DeviceModel>();
}

int DeviceManager::getDeviceCount() const
{
    return this->devices.count();
}

const QSharedPointer<CasparDevice> DeviceManager::getDeviceByName(const QString& name) const
{
    return this->devices.value(name);
}

bool DeviceManager::isChannelLocked(const QString& deviceName, int channel) const
{
    if (this->globalLockedChannels.contains(channel))
        return true;

    if (this->lockedChannels.contains(deviceName))
        return this->lockedChannels.value(deviceName).contains(channel);

    return false;
}

void DeviceManager::toggleChannelLock(const QString& deviceName, int channel)
{
    // Cancel any active timed lock when manually toggling.
    clearTimedLock(deviceName, channel);

    if (this->lockedChannels[deviceName].contains(channel))
        this->lockedChannels[deviceName].remove(channel);
    else
        this->lockedChannels[deviceName].insert(channel);

    bool locked = isChannelLocked(deviceName, channel);
    emit channelLockChanged(deviceName, channel, locked);
}

void DeviceManager::toggleGlobalChannelLock(int channel)
{
    if (this->globalLockedChannels.contains(channel))
        this->globalLockedChannels.remove(channel);
    else
        this->globalLockedChannels.insert(channel);

    bool globalLocked = this->globalLockedChannels.contains(channel);

    // Emit for each device so UI updates all rows.
    foreach (const QString& deviceName, this->devices.keys())
        emit channelLockChanged(deviceName, channel, globalLocked || this->lockedChannels.value(deviceName).contains(channel));

    // Emit with empty device name to signal global change.
    emit channelLockChanged(QString(), channel, globalLocked);
}

QSet<int> DeviceManager::getLockedChannels(const QString& deviceName) const
{
    return this->lockedChannels.value(deviceName);
}

QSet<int> DeviceManager::getGlobalLockedChannels() const
{
    return this->globalLockedChannels;
}

void DeviceManager::setTimedChannelLock(const QString& deviceName, int channel, int durationSecs)
{
    // Clear any existing timed lock on this channel.
    clearTimedLock(deviceName, channel);

    // Lock the channel.
    this->lockedChannels[deviceName].insert(channel);

    // Record timed lock data.
    TimedLockEntry entry;
    entry.elapsed.start();
    entry.durationSecs = durationSecs;
    this->timedLocks[deviceName][channel] = entry;

    // Start the master tick timer if not already running.
    if (!this->tickTimer->isActive())
        this->tickTimer->start();

    emit channelLockChanged(deviceName, channel, true);
    emit timedLockTick(deviceName, channel, durationSecs);
}

bool DeviceManager::isTimedLock(const QString& deviceName, int channel) const
{
    return this->timedLocks.contains(deviceName) &&
           this->timedLocks.value(deviceName).contains(channel);
}

int DeviceManager::getRemainingLockSeconds(const QString& deviceName, int channel) const
{
    if (!isTimedLock(deviceName, channel))
        return 0;

    const TimedLockEntry& entry = this->timedLocks.value(deviceName).value(channel);
    int elapsed = static_cast<int>(entry.elapsed.elapsed() / 1000);
    int remaining = entry.durationSecs - elapsed;
    return (remaining > 0) ? remaining : 0;
}

void DeviceManager::clearTimedLock(const QString& deviceName, int channel)
{
    if (this->timedLocks.contains(deviceName))
    {
        this->timedLocks[deviceName].remove(channel);
        if (this->timedLocks[deviceName].isEmpty())
            this->timedLocks.remove(deviceName);
    }

    // Stop the tick timer if no timed locks remain.
    if (this->timedLocks.isEmpty() && this->tickTimer->isActive())
        this->tickTimer->stop();
}

void DeviceManager::timedLockTickUpdate()
{
    // Collect expired entries to process after iteration.
    QList<QPair<QString, int>> expired;

    for (auto deviceIt = this->timedLocks.begin(); deviceIt != this->timedLocks.end(); ++deviceIt)
    {
        const QString& deviceName = deviceIt.key();
        for (auto channelIt = deviceIt.value().begin(); channelIt != deviceIt.value().end(); ++channelIt)
        {
            int channel = channelIt.key();
            int elapsed = static_cast<int>(channelIt.value().elapsed.elapsed() / 1000);
            int remaining = channelIt.value().durationSecs - elapsed;

            if (remaining <= 0)
                expired.append({deviceName, channel});
            else
                emit timedLockTick(deviceName, channel, remaining);
        }
    }

    // Unlock expired timed locks.
    for (const auto& pair : expired)
    {
        clearTimedLock(pair.first, pair.second);
        this->lockedChannels[pair.first].remove(pair.second);

        bool locked = isChannelLocked(pair.first, pair.second);
        emit channelLockChanged(pair.first, pair.second, locked);
    }
}
