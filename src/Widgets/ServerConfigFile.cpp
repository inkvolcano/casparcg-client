#include "ServerConfigFile.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "ServerProcessControl.h"

#include "Models/DeviceModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QPair>

ServerConfigXml::Document ServerConfigFile::load(const QString& deviceName)
{
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (model == nullptr)
        return ServerConfigXml::Document();

    const QString executable = ServerProcessControl::serverExecutableFor(model->getServerPath());
    if (executable.isEmpty())
        return ServerConfigXml::Document();

    QFile file(QDir(QFileInfo(executable).absolutePath()).filePath("casparcg.config"));
    if (!file.open(QIODevice::ReadOnly))
        return ServerConfigXml::Document();

    return ServerConfigXml::parse(QString::fromUtf8(file.readAll()));
}

int ServerConfigFile::scannerPort(const QString& deviceName)
{
    static QHash<QString, QPair<qint64, int>> known;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto found = known.constFind(deviceName);
    if (found != known.constEnd() && now - found.value().first < 60000)
        return found.value().second;

    int port = 0;
    const ServerConfigXml::Document document = load(deviceName);
    if (document.ok)
        port = ServerConfigXml::value(document.root, "amcp/media-server/port").trimmed().toInt();
    if (port <= 0 || port > 65535)
        port = DatabaseManager::getInstance().getConfigurationByName("ScannerPort").getValue().trimmed().toInt();
    if (port <= 0 || port > 65535)
        port = 8000;

    known.insert(deviceName, qMakePair(now, port));
    return port;
}
