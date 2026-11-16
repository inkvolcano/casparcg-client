#include "LoudnessMonitor.h"

#include "DeviceManager.h"
#include "Global.h"
#include "OscSubscription.h"
#include "ReferenceCapture.h"
#include "ReferenceCaptureRules.h"
#include "SafeStart.h"

#include "NdiManager.h"
#include "NdiReceiver.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <algorithm>
#include <limits>

namespace
{
    const int TICK_MS = 250;
    const int NDI_SILENT_MS = 5000;     // no audio from NDI this long: back to the estimate
    const int NDI_LOOK_MS = 10000;      // how often a declared but unseen source is looked for

    QHash<QString, LoudnessMonitor*>& registry()
    {
        static QHash<QString, LoudnessMonitor*> monitors;
        return monitors;
    }

    qint64 now()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }
}

LoudnessMonitor* LoudnessMonitor::forChannel(const QString& server, int channel)
{
    const QString key = QString("%1/%2").arg(server).arg(channel);
    LoudnessMonitor* monitor = registry().value(key, nullptr);
    if (monitor == nullptr)
    {
        monitor = new LoudnessMonitor(server, channel);
        registry().insert(key, monitor);
    }
    return monitor;
}

QList<LoudnessMonitor*> LoudnessMonitor::started()
{
    QList<LoudnessMonitor*> list;
    for (LoudnessMonitor* monitor : registry())
    {
        if (monitor->isStarted())
            list.append(monitor);
    }
    std::sort(list.begin(), list.end(), [](LoudnessMonitor* a, LoudnessMonitor* b) {
        return a->server() != b->server() ? a->server() < b->server() : a->channel() < b->channel();
    });
    return list;
}

LoudnessMonitor::LoudnessMonitor(const QString& server, int channel)
    : QObject(), serverName(server), channelNumber(channel)
{
    // The receiver's thread uses the NDI library, which goes when the client
    // does: the thread is stopped and waited for first.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        QThread* thread = this->ndiThread.data();
        if (!this->receiver.isNull())
            this->receiver->requestStop();
        if (thread != nullptr)
            thread->wait(2000);
    });
}

void LoudnessMonitor::start()
{
    if (this->running)
        return;
    this->running = true;

    // NDI only for a channel whose config declares it, and never in a safe start.
    if (!SafeStart::active())
        this->ndiName = ReferenceCapture::declaredNdiName(this->serverName, this->channelNumber, nullptr, &this->ndiDeclared);

    subscribeOsc();

    this->timer = new QTimer(this);
    QObject::connect(this->timer, &QTimer::timeout, this, [this]() { tick(); });
    this->timer->start(TICK_MS);

    if (this->ndiDeclared)
        lookForNdi();
}

void LoudnessMonitor::subscribeOsc()
{
    delete this->subscription;
    this->subscription = nullptr;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->serverName);
    if (device == nullptr)
        return;

    QString filter = Osc::AUDIOCHANNEL_FILTER;
    filter.replace("#IPADDRESS#", device->resolveIpAddress()).replace("#CHANNEL#", QString::number(this->channelNumber));
    this->subscription = new OscSubscription(filter, this);
    QObject::connect(this->subscription, &OscSubscription::subscriptionReceived, this,
                     [this](const QString&, const QList<QVariant>& arguments) {
        QList<double> peaks;
        for (const QVariant& value : arguments)
            peaks.append(qMax(0.0, value.toDouble() / double(std::numeric_limits<int32_t>::max())));
        this->lastOscMs = now();
        this->estimate.add(peaks, this->lastOscMs);
    });
}

void LoudnessMonitor::lookForNdi()
{
    this->lastNdiLookMs = now();
    if (!NdiManager::getInstance().initialize())
        return;

    QStringList names;
    for (const NdiSourceInfo& source : NdiManager::getInstance().getSources())
        names << source.name;
    const QString source = ReferenceCaptureRules::findSource(names, this->ndiName);
    if (!source.isEmpty())
        startNdi(source);
}

void LoudnessMonitor::startNdi(const QString& source)
{
    const NDIlib_v5* ndi = NdiManager::getInstance().api();
    if (ndi == nullptr || !this->receiver.isNull())
        return;

    // Audio only, played nowhere, on its own thread - as an NDI viewer runs one.
    QThread* thread = new QThread();
    thread->setObjectName("NdiLoudness");
    NdiReceiver* r = new NdiReceiver(ndi);
    r->setSource(NdiSourceInfo(source));
    r->setMuted(true);
    r->setBandwidth(NDIlib_recv_bandwidth_audio_only);
    r->setLoudness(true);
    r->moveToThread(thread);

    QObject::connect(thread, &QThread::started, r, &NdiReceiver::run);
    QObject::connect(r, &NdiReceiver::finished, thread, &QThread::quit);
    QObject::connect(thread, &QThread::finished, r, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QObject::connect(r, &NdiReceiver::loudnessMeasured, this, [this](double m, double s, double i) {
        this->lastNdiMs = now();
        this->ndiMomentary = m;
        this->ndiShort = s;
        this->ndiIntegrated = i;
        if (!this->fromNdi)
        {
            this->fromNdi = true;
            qInfo("Loudness: %s channel %d measured from NDI (%s)", qPrintable(this->serverName), this->channelNumber,
                  qPrintable(this->ndiSource));
        }
    });

    this->receiver = r;
    this->ndiThread = thread;
    this->ndiSource = source;
    this->lastNdiMs = now();
    thread->start();
}

void LoudnessMonitor::stopNdi()
{
    if (!this->receiver.isNull())
        this->receiver->requestStop();
    this->receiver = nullptr;
    if (this->fromNdi)
        qInfo("Loudness: %s channel %d - no NDI audio for 5 s, estimating from OSC", qPrintable(this->serverName), this->channelNumber);
    this->fromNdi = false;
    this->ndiMomentary = this->ndiShort = this->ndiIntegrated = Loudness::none();
}

void LoudnessMonitor::tick()
{
    const qint64 t = now();

    if (!this->receiver.isNull() && t - this->lastNdiMs > NDI_SILENT_MS)
        stopNdi();
    else if (this->receiver.isNull() && this->ndiDeclared && t - this->lastNdiLookMs > NDI_LOOK_MS)
        lookForNdi();

    // A server added or reconnected after start: subscribe again.
    if (this->subscription == nullptr)
        subscribeOsc();

    emit changed();
}

void LoudnessMonitor::reset()
{
    this->estimate.reset();
    if (!this->receiver.isNull())
        this->receiver->resetLoudness();
    this->ndiIntegrated = Loudness::none();
    emit changed();
}

double LoudnessMonitor::momentary() const
{
    return this->fromNdi ? this->ndiMomentary : Loudness::none();
}

double LoudnessMonitor::shortTerm() const
{
    return this->fromNdi ? this->ndiShort : this->estimate.shortTerm(now());
}

double LoudnessMonitor::integrated() const
{
    return this->fromNdi ? this->ndiIntegrated : this->estimate.integrated();
}

QString LoudnessMonitor::sourceText() const
{
    if (this->fromNdi)
        return QString("R128 from NDI (%1)").arg(this->ndiSource);
    if (this->ndiDeclared && this->receiver.isNull())
        return "Estimate from OSC peaks - NDI output declared but not found yet";
    return "Estimate from OSC peaks (no NDI output on this channel) - reads high on speech";
}
