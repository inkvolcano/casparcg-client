#pragma once

// The run sheet's timing column (build 304): when each item starts.
//
// The lengths are the timing bar's own (RundownTimingBar::timedItems), so the
// sheet and the bar always agree. An item the bar does not count on its own -
// inside a group that has a planned Duration, or disabled - has no start of
// its own (-1 in, empty out).
//
// With a hard out, starts are times of day, back-timed so the last item ends at
// the hard out; without one they are offsets from the top, "+4:05".
// Tested by tools/test-runsheet.

#include "RundownTiming.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace RunSheetRules
{
    // `lengths`: seconds per row in rundown order, -1 for a row with no length
    // of its own. `hardOutSeconds`: seconds since midnight, or -1 for none.
    inline QStringList starts(const QList<double>& lengths, int hardOutSeconds)
    {
        double total = 0;
        for (double length : lengths)
        {
            if (length >= 0)
                total += length;
        }

        QStringList out;
        double offset = 0;
        for (double length : lengths)
        {
            if (length < 0)
            {
                out << QString();
                continue;
            }

            if (hardOutSeconds >= 0)
            {
                int at = hardOutSeconds - int(total - offset + 0.5);
                while (at < 0)
                    at += 24 * 3600;
                out << RundownTiming::formatTimeOfDay(at % (24 * 3600));
            }
            else
            {
                out << "+" + RundownTiming::formatLength(offset);
            }
            offset += length;
        }
        return out;
    }

    inline double total(const QList<double>& lengths)
    {
        double sum = 0;
        for (double length : lengths)
        {
            if (length >= 0)
                sum += length;
        }
        return sum;
    }
}
