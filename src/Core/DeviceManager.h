#pragma once

#include "Shared.h"
#include "Models/DeviceModel.h"

#include "CasparDevice.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>

class CORE_EXPORT DeviceManager : public QObject
{
    Q_OBJECT

    public:
        explicit DeviceManager();

        static DeviceManager& getInstance();

        void initialize();
        void uninitialize();
        void refresh();

        QList<DeviceModel> getDeviceModels() const;
        const QSharedPointer<DeviceModel> getDeviceModelByName(const QString& name) const;
        const QSharedPointer<DeviceModel> getDeviceModelByAddress(const QString& address) const;

        int getDeviceCount() const;
        const QSharedPointer<CasparDevice> getDeviceByName(const QString& name) const;

        bool isChannelLocked(const QString& deviceName, int channel) const;
        void toggleChannelLock(const QString& deviceName, int channel);
        void toggleGlobalChannelLock(int channel);
        QSet<int> getLockedChannels(const QString& deviceName) const;
        QSet<int> getGlobalLockedChannels() const;

        void setTimedChannelLock(const QString& deviceName, int channel, int durationSecs);
        bool isTimedLock(const QString& deviceName, int channel) const;
        int getRemainingLockSeconds(const QString& deviceName, int channel) const;

        Q_SIGNAL void deviceRemoved();
        Q_SIGNAL void deviceAdded(CasparDevice&);
        Q_SIGNAL void channelLockChanged(const QString& deviceName, int channel, bool locked);
        Q_SIGNAL void timedLockTick(const QString& deviceName, int channel, int remainingSecs);

    private:
        void clearTimedLock(const QString& deviceName, int channel);
        void timedLockTickUpdate();

        QMap<QString, DeviceModel> deviceModels;
        QMap<QString, QSharedPointer<CasparDevice>> devices;
        QMap<QString, QSet<int>> lockedChannels;
        QSet<int> globalLockedChannels;

        // Timed locks: per-device, per-channel countdown data.
        struct TimedLockEntry {
            QElapsedTimer elapsed;
            int durationSecs = 0;
        };
        QMap<QString, QMap<int, TimedLockEntry>> timedLocks;
        QTimer* tickTimer = nullptr;
};

