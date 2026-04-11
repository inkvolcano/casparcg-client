#pragma once

#include "Shared.h"
#include "Models/DeviceModel.h"

#include "CasparDevice.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

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

        Q_SIGNAL void deviceRemoved();
        Q_SIGNAL void deviceAdded(CasparDevice&);
        Q_SIGNAL void channelLockChanged(const QString& deviceName, int channel, bool locked);

    private:
        QMap<QString, DeviceModel> deviceModels;
        QMap<QString, QSharedPointer<CasparDevice>> devices;
        QMap<QString, QSet<int>> lockedChannels;
        QSet<int> globalLockedChannels;
};

