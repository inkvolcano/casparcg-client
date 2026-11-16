#include "ReferenceCapture.h"

#include "ServerConfigFile.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "ReferenceImage.h"
#include "SafeStart.h"
#include "ServerConfigXml.h"
#include "ServerProcessControl.h"

#include "NdiManager.h"
#include "NdiReceiver.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <QtGui/QImage>

#include <memory>

namespace
{
    const int NDI_DISCOVERY_MS = 4000;
    const int NDI_FRAME_MS = 5000;
    const int STILL_WAIT_MS = 8000;
}

ReferenceCaptureRules::Setting ReferenceCapture::setting()
{
    return ReferenceCaptureRules::settingFrom(DatabaseManager::getInstance().getConfigurationByName("ReferencePictureSource").getValue());
}

void ReferenceCapture::setSetting(ReferenceCaptureRules::Setting setting)
{
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "ReferencePictureSource", ReferenceCaptureRules::settingText(setting)));
}

int ReferenceCapture::channelFor(const QString& deviceName, int itemChannel)
{
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (model != nullptr && model->getPreviewChannel() > 0)
        return model->getPreviewChannel();
    return itemChannel;
}

ReferenceCaptureRules::Situation ReferenceCapture::situationFor(const QString& deviceName, int channel)
{
    ReferenceCaptureRules::Situation s;
    s.channel = channel;
    s.server = deviceName;

    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (model == nullptr)
        return s;

    // The channel's NDI output, from the server's own casparcg.config.
    s.ndiName = declaredNdiName(deviceName, channel, &s.configRead, &s.ndiDeclared);

    s.ndiAvailable = !SafeStart::active() && NdiManager::getInstance().initialize();
    if (s.ndiDeclared && s.ndiAvailable)
    {
        QStringList names;
        for (const NdiSourceInfo& source : NdiManager::getInstance().getSources())
            names << source.name;
        s.ndiSource = ReferenceCaptureRules::findSource(names, s.ndiName);
    }

    const QString mediaPath = model->getMediaPath().trimmed();
    s.mediaFolderSet = !mediaPath.isEmpty() && QDir::isAbsolutePath(mediaPath) && QFileInfo(mediaPath).isDir();
    return s;
}

QString ReferenceCapture::declaredNdiName(const QString& deviceName, int channel, bool* configRead, bool* declared)
{
    if (configRead != nullptr)
        *configRead = false;
    if (declared != nullptr)
        *declared = false;

    const ServerConfigXml::Document document = ServerConfigFile::load(deviceName);
    if (!document.ok)
        return QString();

    if (configRead != nullptr)
        *configRead = true;
    const QList<ServerConfigXml::NodePtr> channels =
        ServerConfigXml::elements(ServerConfigXml::child(document.root, "channels"), "channel");
    if (channel < 1 || channel > channels.count())
        return QString();

    const ServerConfigXml::NodePtr ndi = ServerConfigXml::find(channels.at(channel - 1), "consumers/ndi");
    if (declared != nullptr)
        *declared = ndi != nullptr;
    return ServerConfigXml::value(ndi, "name");
}

void ReferenceCapture::capture(QObject* context, const QString& deviceName, int itemChannel, const Done& done)
{
    const int channel = channelFor(deviceName, itemChannel);
    if (channel <= 0)
    {
        done(QString(), QString("The item has no channel to take a picture of."), QString());
        return;
    }

    ReferenceCaptureRules::Situation situation = situationFor(deviceName, channel);

    // NDI discovery may not have seen the source yet when NDI has just started:
    // give it a few seconds before deciding there is none.
    const ReferenceCaptureRules::Setting chosen = setting();
    if (situation.ndiDeclared && situation.ndiAvailable && situation.ndiSource.isEmpty()
        && chosen != ReferenceCaptureRules::Setting::Server)
    {
        QPointer<QObject> guard(context);
        auto waited = std::make_shared<QElapsedTimer>();
        waited->start();
        QTimer* poll = new QTimer(context);
        poll->setInterval(250);
        QObject::connect(poll, &QTimer::timeout, context, [poll, waited, guard, deviceName, channel, chosen, done]() {
            ReferenceCaptureRules::Situation now = situationFor(deviceName, channel);
            if (now.ndiSource.isEmpty() && waited->elapsed() < NDI_DISCOVERY_MS)
                return;
            poll->stop();
            poll->deleteLater();
            if (guard.isNull())
                return;
            const ReferenceCaptureRules::Plan plan = ReferenceCaptureRules::plan(chosen, now);
            if (plan.route == ReferenceCaptureRules::Route::Ndi)
                captureNdi(guard, now.ndiSource, done);
            else if (plan.route == ReferenceCaptureRules::Route::Server)
                captureStill(guard, deviceName, channel, done);
            else
                done(QString(), QString("Nothing to take a picture from: ") + plan.why, QString());
        });
        poll->start();
        return;
    }

    const ReferenceCaptureRules::Plan plan = ReferenceCaptureRules::plan(chosen, situation);
    if (plan.route == ReferenceCaptureRules::Route::Ndi)
        captureNdi(context, situation.ndiSource, done);
    else if (plan.route == ReferenceCaptureRules::Route::Server)
        captureStill(context, deviceName, channel, done);
    else
        done(QString(), QString("Nothing to take a picture from: ") + plan.why, QString());
}

void ReferenceCapture::captureNdi(QObject* context, const QString& sourceName, const Done& done)
{
    const NDIlib_v5* ndi = NdiManager::getInstance().api();
    if (ndi == nullptr)
    {
        done(QString(), QString("NDI is not available here."), QString());
        return;
    }

    // A receiver of its own, for one frame, on its own thread - the same way an
    // NDI viewer runs one - stopped as soon as the frame is in.
    QThread* thread = new QThread();
    thread->setObjectName("NdiReference");
    NdiReceiver* receiver = new NdiReceiver(ndi);
    NdiSourceInfo source(sourceName);
    receiver->setSource(source);
    receiver->setMuted(true);
    receiver->moveToThread(thread);

    QObject::connect(thread, &QThread::started, receiver, &NdiReceiver::run);
    QObject::connect(receiver, &NdiReceiver::finished, thread, &QThread::quit);
    QObject::connect(thread, &QThread::finished, receiver, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    auto answered = std::make_shared<bool>(false);
    QPointer<QObject> guard(context);
    auto finish = [receiver, answered]() {
        *answered = true;
        receiver->requestStop();
    };

    QObject::connect(receiver, &NdiReceiver::videoFrameReceived, context, [guard, answered, finish, done, sourceName](const QImage& image) {
        if (*answered)
            return;
        finish();
        if (guard.isNull())
            return;
        const QString picture = ReferenceImage::encode(image);
        if (picture.isEmpty())
            done(QString(), QString("The NDI frame from %1 could not be used as a picture.").arg(sourceName), QString());
        else
            done(picture, QString(), QString("NDI, %1").arg(sourceName));
    });

    QTimer::singleShot(NDI_FRAME_MS, context, [guard, answered, finish, done, sourceName]() {
        if (*answered)
            return;
        finish();
        if (!guard.isNull())
            done(QString(), QString("No frame came from the NDI source %1 within %2 seconds.").arg(sourceName).arg(NDI_FRAME_MS / 1000), QString());
    });

    thread->start();
}

void ReferenceCapture::captureStill(QObject* context, const QString& deviceName, int channel, const Done& done)
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(deviceName);
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (device == nullptr || model == nullptr || !device->isConnected())
    {
        done(QString(), QString("%1 is not connected, so it cannot write a still.").arg(deviceName), QString());
        return;
    }

    const QString name = ReferenceCaptureRules::stillName(QDateTime::currentMSecsSinceEpoch());
    const QString path = QDir(model->getMediaPath().trimmed()).filePath(name + ".png");
    device->sendCommand(ReferenceCaptureRules::stillCommand(channel, name));

    // The server writes the file on a thread of its own: wait until it is there
    // and has stopped growing, then read it and take it away again - it is this
    // client's file, made a moment ago, not anything of the server's.
    QPointer<QObject> guard(context);
    auto waited = std::make_shared<QElapsedTimer>();
    waited->start();
    auto lastSize = std::make_shared<qint64>(-1);
    QTimer* poll = new QTimer(context);
    poll->setInterval(200);
    QObject::connect(poll, &QTimer::timeout, context, [poll, waited, lastSize, guard, path, deviceName, channel, done]() {
        const QFileInfo info(path);
        const qint64 size = info.exists() ? info.size() : -1;
        const bool settled = size > 0 && size == *lastSize;
        *lastSize = size;

        if (!settled && waited->elapsed() < STILL_WAIT_MS)
            return;

        poll->stop();
        poll->deleteLater();

        if (!settled)
        {
            if (!guard.isNull())
                done(QString(), QString("%1 did not write a still of channel %2 to %3 within %4 seconds.")
                                    .arg(deviceName).arg(channel).arg(QDir::toNativeSeparators(QFileInfo(path).absolutePath()))
                                    .arg(STILL_WAIT_MS / 1000), QString());
            return;
        }

        const QImage image(path);
        QFile::remove(path);
        if (guard.isNull())
            return;
        const QString picture = image.isNull() ? QString() : ReferenceImage::encode(image);
        if (picture.isEmpty())
            done(QString(), QString("The still from %1 could not be read as a picture.").arg(deviceName), QString());
        else
            done(picture, QString(), QString("a still from %1, channel %2").arg(deviceName).arg(channel));
    });
    poll->start();
}
