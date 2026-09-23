#include "AsRunLogWriter.h"

#include "AsRunLog.h"
#include "DeviceManager.h"
#include "CasparDevice.h"
#include "Models/DeviceModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>

AsRunLogWriter::AsRunLogWriter(QObject* parent)
    : QObject(parent)
{
    // Servers are added after the window is built, and again whenever Settings
    // changes them, so every one is picked up as it arrives.
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice&)),
                     this, SLOT(deviceAdded(CasparDevice&)));

    for (const DeviceModel& model : DeviceManager::getInstance().getDeviceModels())
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (device != nullptr)
            deviceAdded(*device);
    }
}

QString AsRunLogWriter::folder()
{
    return QString("%1/.CasparCG/Client/AsRun").arg(QDir::homePath());
}

void AsRunLogWriter::deviceAdded(CasparDevice& device)
{
    // UniqueConnection: a server is announced more than once over a session, and
    // a second connection would write every line twice.
    QObject::connect(&device, &AmcpDevice::messageWritten, this, &AsRunLogWriter::commandSent, Qt::UniqueConnection);
}

QString AsRunLogWriter::serverNameOf(const QObject* device)
{
    for (const DeviceModel& model : DeviceManager::getInstance().getDeviceModels())
    {
        if (DeviceManager::getInstance().getDeviceByName(model.getName()).data() == device)
            return model.getName();
    }

    const AmcpDevice* amcp = qobject_cast<const AmcpDevice*>(device);
    return amcp != nullptr ? QString("%1:%2").arg(amcp->getAddress()).arg(amcp->getPort()) : QString();
}

void AsRunLogWriter::commandSent(const QString& message)
{
    // Most of what is sent - listings, thumbnails, mixer moves - is not as-run,
    // so the command is read first and the server looked up only when it is.
    AsRun::Entry entry;
    if (!AsRun::fromCommand(message, entry))
        return;

    write(AsRun::csvLine(QDateTime::currentDateTime(), serverNameOf(sender()), entry, message));
}

void AsRunLogWriter::write(const QString& line)
{
    const QDate today = QDate::currentDate();

    if (!this->file.isOpen() || today != this->fileDate)
    {
        this->file.close();

        const QDir dir(folder());
        QDir().mkpath(dir.path());

        this->file.setFileName(dir.filePath(AsRun::fileName(today)));
        if (!this->file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            qWarning("As-run log: could not open %s", qPrintable(this->file.fileName()));
            return;
        }
        this->fileDate = today;

        // A new file starts with a byte-order mark, so a spreadsheet opens names
        // with accents correctly, and a header row.
        if (this->file.size() == 0)
        {
            this->file.write("\xEF\xBB\xBF");
            this->file.write((AsRun::header() + "\r\n").toUtf8());
        }

        for (const QString& name : AsRun::expired(dir.entryList(QStringList() << "AsRun_*.csv", QDir::Files), today))
            QFile::remove(dir.filePath(name));
    }

    this->file.write((line + "\r\n").toUtf8());
    this->file.flush();
}
