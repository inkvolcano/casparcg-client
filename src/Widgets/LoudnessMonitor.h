#pragma once

#include "Shared.h"

#include "LoudnessMeter.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QThread;
class QTimer;
QT_END_NAMESPACE

class NdiReceiver;
class OscSubscription;

// The loudness of one server channel (build 312), measured for as long as the
// client runs once something has asked for it.
//
// From the channel's NDI output when the server's casparcg.config declares one
// and it can be found on the network: a real EBU R128 reading (Common/
// LoudnessMeter.h) from an audio-only receiver that plays nothing. Otherwise -
// and while NDI is being looked for, or after its audio stops for 5 s - an
// estimate from the OSC peak levels the audio meters already read, always
// labelled as one. NDI is never started for a channel that does not declare it.
class WIDGETS_EXPORT LoudnessMonitor : public QObject
{
    Q_OBJECT

    public:
        // One per server channel, made on first ask and kept, so the integrated
        // reading survives the meters panel being rebuilt.
        static LoudnessMonitor* forChannel(const QString& server, int channel);
        static QList<LoudnessMonitor*> started();

        void start();
        void reset();

        const QString& server() const { return this->serverName; }
        int channel() const { return this->channelNumber; }
        bool isStarted() const { return this->running; }

        // True while the readings are R128 from NDI; false for the OSC estimate.
        bool measured() const { return this->fromNdi; }
        double momentary() const;
        double shortTerm() const;
        double integrated() const;
        QString sourceText() const;

        Q_SIGNAL void changed();

    private:
        LoudnessMonitor(const QString& server, int channel);

        QString serverName;
        int channelNumber = 0;
        bool running = false;

        // NDI.
        bool fromNdi = false;
        bool ndiDeclared = false;
        QString ndiName;
        QString ndiSource;
        QPointer<NdiReceiver> receiver;
        QPointer<QThread> ndiThread;
        qint64 lastNdiMs = 0;
        qint64 lastNdiLookMs = 0;
        double ndiMomentary = Loudness::none();
        double ndiShort = Loudness::none();
        double ndiIntegrated = Loudness::none();

        // OSC.
        OscSubscription* subscription = nullptr;
        Loudness::PeakEstimate estimate;
        qint64 lastOscMs = 0;

        QTimer* timer = nullptr;

        void subscribeOsc();
        void lookForNdi();
        void startNdi(const QString& source);
        void stopNdi();
        void tick();
};
