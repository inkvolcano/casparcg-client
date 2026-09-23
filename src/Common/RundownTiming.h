#pragma once

// Rundown timing: how long a rundown runs, how much is left from the selected
// item, and whether it will finish before a hard-out time the operator types.
//
// The user's choices (2026-09-23): a bar under each rundown, back-timed to a
// hard-out time of day, saved with the rundown.
//
// What an item counts:
//   - a movie or audio clip: its Length setting if one is set, otherwise the
//     clip's own length from the Library, less where it starts (Seek);
//   - any other item: its Duration setting, if it has one;
//   - a group: its own Duration, if one is typed - the planned length of the
//     segment - otherwise the sum of what is in it;
//   - a disabled item, and everything in a disabled group: nothing.
//
// The arithmetic is here; src/Widgets/Rundown/RundownTimingBar walks the rundown.
// tools/test-rundowntiming checks it.

#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace RundownTiming
{
    static const int DAY = 24 * 60 * 60;

    // "21:00", "21:00:00" or "9:05" as seconds since midnight; -1 when empty or not
    // a time of day.
    inline int parseHardOut(const QString& text)
    {
        static const QRegularExpression pattern("^\\s*(\\d{1,2}):(\\d{2})(?::(\\d{2}))?\\s*$");
        const QRegularExpressionMatch match = pattern.match(text);
        if (!match.hasMatch())
            return -1;

        const int h = match.captured(1).toInt();
        const int m = match.captured(2).toInt();
        const int s = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
        if (h > 23 || m > 59 || s > 59)
            return -1;

        return h * 3600 + m * 60 + s;
    }

    inline QString formatTimeOfDay(int seconds)
    {
        seconds = ((seconds % DAY) + DAY) % DAY;
        return QString("%1:%2:%3").arg(seconds / 3600, 2, 10, QChar('0'))
                                  .arg((seconds % 3600) / 60, 2, 10, QChar('0'))
                                  .arg(seconds % 60, 2, 10, QChar('0'));
    }

    // A length: "4:05" under an hour, "1:04:05" from an hour, "0:00" for nothing.
    inline QString formatLength(double seconds)
    {
        const qint64 total = qMax<qint64>(0, qRound64(seconds));
        const qint64 h = total / 3600;
        const qint64 m = (total % 3600) / 60;
        const qint64 s = total % 60;

        if (h > 0)
            return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }

    // Seconds the show ends after the hard out: positive is over, negative under.
    // A hard out more than twelve hours behind now is taken to be tomorrow's, and
    // more than twelve hours ahead yesterday's, so 00:30 works for a show at 23:00.
    inline double overBy(int nowSeconds, double remainingSeconds, int hardOutSeconds)
    {
        double hardOut = hardOutSeconds;
        while (hardOut < nowSeconds - DAY / 2)
            hardOut += DAY;
        while (hardOut > nowSeconds + DAY / 2)
            hardOut -= DAY;

        return (nowSeconds + remainingSeconds) - hardOut;
    }

    // A clip: the Length setting in frames if set, otherwise the clip's length
    // less its Seek (also frames). Nothing when the frame rate is not known.
    inline double clipSeconds(double clipLengthSeconds, int seekFrames, int lengthFrames, double fps)
    {
        if (lengthFrames > 0)
            return fps > 0 ? lengthFrames / fps : 0;

        const double seek = (fps > 0 && seekFrames > 0) ? seekFrames / fps : 0;
        return qMax(0.0, clipLengthSeconds - seek);
    }

    // A Duration setting, in milliseconds or in frames by the client's setting.
    inline double durationSeconds(int value, bool inMilliseconds, double fps)
    {
        if (value <= 0)
            return 0;
        if (inMilliseconds)
            return value / 1000.0;
        return fps > 0 ? value / fps : 0;
    }

    // The hard out a saved rundown carries, if any.
    inline QString hardOutIn(const QString& rundownXml)
    {
        static const QRegularExpression element("<hardout>\\s*([^<]*?)\\s*</hardout>");
        const QRegularExpressionMatch match = element.match(rundownXml);
        if (!match.hasMatch())
            return QString();

        const int seconds = parseHardOut(match.captured(1));
        return seconds < 0 ? QString() : formatTimeOfDay(seconds);
    }
}
