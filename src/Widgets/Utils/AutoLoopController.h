#pragma once

#include "../Shared.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

// Per-item auto-loop driver: fires a `firePlay` signal every `delaySeconds` while active,
// and emits an AutoLoopCountdownEvent on the EventManager each tick so the Activity panel
// can render a per-item countdown.
class WIDGETS_EXPORT AutoLoopController : public QObject
{
    Q_OBJECT

    public:
        explicit AutoLoopController(QObject* parent = nullptr);

        void setContext(int channel, int videolayer, const QString& label, const QString& itemType);
        void setDelaySeconds(int delaySeconds);
        int getDelaySeconds() const;

        // Arms the countdown: sets remaining = delaySeconds, starts 1 Hz ticks.
        // Does NOT fire play immediately — the caller has just fired play, or intends to fire on tick 0.
        void restartCountdown();

        // Stops the countdown and emits an inactive event so the Activity panel row is removed.
        void stop();

        bool isActive() const;
        int getRemainingSeconds() const;

    Q_SIGNALS:
        void firePlay();

    private Q_SLOTS:
        void onTick();

    private:
        QTimer timer;
        int delaySeconds = 5;
        int remaining = 0;
        bool active = false;
        int channel = 1;
        int videolayer = 10;
        QString label;
        QString itemType;

        void emitCountdown();
};
