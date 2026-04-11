#include "OscDeviceManager.h"
#include "DatabaseManager.h"
#include "Global.h"

#include <QtCore/QSharedPointer>

Q_GLOBAL_STATIC(OscDeviceManager, oscDeviceManager)

OscDeviceManager::OscDeviceManager()
{
}

OscDeviceManager& OscDeviceManager::getInstance()
{
    return *oscDeviceManager();
}

void OscDeviceManager::initialize()
{
    this->oscSender = QSharedPointer<OscSender>();

    // Bulk-load all config values in a single DB query (replaces 5 individual SELECTs).
    QMap<QString, QString> cfg = DatabaseManager::getInstance().getAllConfigurations();

    QString refreshStr = cfg.value("OscRefreshRate", QString());
    int refreshRate = refreshStr.isEmpty() ? Osc::DEFAULT_REFRESH_RATE : refreshStr.toInt();

    QString oscMonitorPort = cfg.value("OscMonitorPort", QString());
    this->oscMonitorListener = QSharedPointer<OscMonitorListener>(new OscMonitorListener());
    if (cfg.value("EnableOscInputMonitor", QString()) == "true")
        this->oscMonitorListener->start((oscMonitorPort.isEmpty() == true) ? Osc::DEFAULT_MONITOR_PORT : oscMonitorPort.toInt(), refreshRate);

    QString oscControlPort = cfg.value("OscControlPort", QString());
    this->oscControlListener = QSharedPointer<OscControlListener>(new OscControlListener());
    if (cfg.value("EnableOscInputControl", QString()) == "true")
        this->oscControlListener->start((oscControlPort.isEmpty() == true) ? Osc::DEFAULT_CONTROL_PORT : oscControlPort.toInt(), refreshRate * 2);
}

void OscDeviceManager::uninitialize()
{
}

const QSharedPointer<OscSender> OscDeviceManager::getOscSender() const
{
    return this->oscSender;
}

const QSharedPointer<OscMonitorListener> OscDeviceManager::getOscMonitorListener() const
{
    return this->oscMonitorListener;
}
 
const QSharedPointer<OscControlListener> OscDeviceManager::getOscControlListener() const
{
    return this->oscControlListener;
}
