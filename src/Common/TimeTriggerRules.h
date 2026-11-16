#pragma once

// Items that fire at a time of day (build 303).
//
// An item's "Fire at" is a clock time, every day: "19:59:50", or "19:59" for
// the top of that minute. The rows show how long is left either way; the item
// only fires when Rundown > Arm Time Triggers is on, which is off at every
// start - a rundown opened at the wrong moment, or left open overnight, does
// nothing on its own until someone arms it.
//
// The clock is looked at once a second. An item is due when its time is past
// the last look and not past this one, so a second the timer arrived late for
// still fires once, and a time is never fired twice for the same moment.
// Tested by tools/test-timetriggers.

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QTime>

namespace TimeTriggerRules
{
    // "19:59:50" or "19:59" (seconds 0); anything else is no time.
    inline QTime parse(const QString& text)
    {
        const QString t = text.trimmed();
        QTime time = QTime::fromString(t, "HH:mm:ss");
        if (!time.isValid())
            time = QTime::fromString(t, "H:mm:ss");
        if (!time.isValid())
            time = QTime::fromString(t, "HH:mm");
        if (!time.isValid())
            time = QTime::fromString(t, "H:mm");
        return time;
    }

    inline QString normalised(const QString& text)
    {
        const QTime time = parse(text);
        return time.isValid() ? time.toString("HH:mm:ss") : QString();
    }

    // The next moment `at` comes round, from `now`: today, or tomorrow when it
    // has gone by today.
    inline QDateTime next(const QTime& at, const QDateTime& now)
    {
        QDateTime candidate(now.date(), at);
        if (candidate <= now)
            candidate = candidate.addDays(1);
        return candidate;
    }

    // Due when `at` falls after the last look and at or before this one -
    // across midnight too.
    inline bool due(const QTime& at, const QDateTime& lastLook, const QDateTime& now)
    {
        if (!at.isValid() || !lastLook.isValid() || now <= lastLook)
            return false;
        // At most a minute is caught up: a client asleep for an hour does not
        // fire everything it slept through when it wakes.
        if (lastLook.secsTo(now) > 60)
            return false;
        QDateTime moment(lastLook.date(), at);
        if (moment <= lastLook)
            moment = moment.addDays(1);
        return moment <= now;
    }

    // "in 3:12", "in 1:04:00", "now".
    inline QString countdown(const QTime& at, const QDateTime& now)
    {
        const qint64 seconds = now.secsTo(next(at, now));
        if (seconds <= 0)
            return QString("now");
        const qint64 h = seconds / 3600;
        const qint64 m = (seconds % 3600) / 60;
        const qint64 s = seconds % 60;
        if (h > 0)
            return QString("in %1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QString("in %1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }
}
