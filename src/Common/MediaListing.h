#pragma once

// Reading what a server says it has, and ordering it.
//
// CLS answers with one line per file:
//
//     "AMB"  MOVIE  6445960 20121101160514 643 1/60
//
// which is name, type, size in bytes, when it was written, length in frames, and
// the frame rate as a fraction. The client has always parsed the last two into a
// timecode and thrown the other two away - so the Library could sort by name and
// nothing else, while the size and the date were arriving on every refresh and
// being dropped on the floor.
//
// Sorting by date matters more than it sounds. The question an operator actually
// has is "where is the clip that just landed", and in a folder of four hundred
// files sorted alphabetically that is a search rather than a glance.
//
// Three things about the parsing are worth stating where the code is:
//
//   Names contain spaces, and the quotes are the only thing separating the name
//   from the fields. Splitting on whitespace works until somebody has a file
//   called "OPENER FINAL v2".
//
//   Trailing fields go missing. A still has no frame count, an older server sends
//   fewer columns, and CasparCG's own documentation contains a line with a
//   fourteen-digit timestamp that is not a date. Every field is optional and an
//   absent one has to be absent rather than zero - a clip of unknown length is
//   not a clip of length zero, and sorting has to put it somewhere deliberate.
//
//   The date is the file's own timestamp, not when this client first saw it. That
//   is the more useful of the two and the only one the server can tell us.
//
// Header-only, like PanelFit.h and OgrafManifest.h, so the parsing and the
// ordering can be compiled and tested without a server or a database.

#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace MediaListing
{
    // A missing number, kept distinct from zero. A file of unknown size is not an
    // empty file, and an item of unknown length is not an instant one.
    static const qint64 UNKNOWN = -1;

    struct Entry
    {
        QString name;
        QString type;
        qint64 sizeBytes = UNKNOWN;
        QString timestamp;      // as sent, "YYYYMMDDhhmmss"
        qint64 durationMs = UNKNOWN;

        // Kept because the Library still shows a timecode, and building one needs
        // the rate as well as the length. Everything the line carries is parsed
        // once, here, rather than picked apart again by each caller.
        double fps = 0.0;

        bool isValid() const { return !this->name.isEmpty(); }
        bool hasSize() const { return this->sizeBytes >= 0; }
        bool hasDuration() const { return this->durationMs >= 0; }
    };

    // The name is whatever sits between the first pair of quotes. Anything else -
    // splitting on the first space, on two spaces, on whitespace runs - breaks on
    // a file with a space in its name, which is most of them.
    inline QString nameFrom(const QString& line)
    {
        const int open = line.indexOf('"');
        if (open < 0)
            return QString();

        const int close = line.indexOf('"', open + 1);
        if (close < 0)
            return QString();

        QString name = line.mid(open + 1, close - open - 1);

        // Servers send Windows separators; the whole client works in forward
        // slashes and a name that disagreed would match nothing.
        name.replace("\\", "/");

        return name;
    }

    // "20121101160514" as a date. Anything that is not fourteen digits making a
    // real date is no date at all rather than a wrong one - CasparCG's own
    // documentation has a line reading 22222222222222.
    inline QDateTime dateFrom(const QString& stamp)
    {
        if (stamp.length() != 14)
            return QDateTime();

        for (int i = 0; i < stamp.length(); i++)
        {
            if (!stamp.at(i).isDigit())
                return QDateTime();
        }

        return QDateTime::fromString(stamp, "yyyyMMddHHmmss");
    }

    // Frames and a timebase fraction into milliseconds. The fraction is sent the
    // way ffmpeg writes it - 1/25 is twenty-five frames a second, 100/2997 is
    // 29.97 - so the numerator is the one that has to be on the bottom.
    inline double fpsFrom(const QString& timebase)
    {
        const QStringList parts = timebase.split('/');
        if (parts.size() != 2)
            return 0.0;

        bool numeratorOk = false;
        bool denominatorOk = false;
        const double numerator = parts.at(0).toDouble(&numeratorOk);
        const double denominator = parts.at(1).toDouble(&denominatorOk);

        if (!numeratorOk || !denominatorOk || numerator <= 0.0 || denominator <= 0.0)
            return 0.0;

        return denominator / numerator;
    }

    inline qint64 durationFrom(const QString& frames, const QString& timebase)
    {
        bool framesOk = false;
        const qint64 frameCount = frames.toLongLong(&framesOk);
        if (!framesOk || frameCount < 0)
            return UNKNOWN;

        const double fps = fpsFrom(timebase);
        if (fps <= 0.0)
            return UNKNOWN;

        return static_cast<qint64>((static_cast<double>(frameCount) / fps) * 1000.0 + 0.5);
    }

    inline Entry parseLine(const QString& line)
    {
        Entry entry;

        entry.name = nameFrom(line);
        if (entry.name.isEmpty())
            return entry;

        // Everything after the closing quote is fields, separated by runs of
        // whitespace that the server is not consistent about.
        const int close = line.indexOf('"', line.indexOf('"') + 1);
        const QStringList fields =
            line.mid(close + 1).trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (fields.isEmpty())
            return entry;

        entry.type = fields.at(0);

        if (fields.size() > 1)
        {
            bool ok = false;
            const qint64 size = fields.at(1).toLongLong(&ok);
            if (ok && size >= 0)
                entry.sizeBytes = size;
        }

        if (fields.size() > 2)
        {
            // Kept as sent rather than as a QDateTime, because it goes into the
            // database as text and a round trip through a locale is a way to lose
            // it. dateFrom() is what turns it into a date when one is wanted.
            if (dateFrom(fields.at(2)).isValid())
                entry.timestamp = fields.at(2);
        }

        if (fields.size() > 4)
        {
            entry.durationMs = durationFrom(fields.at(3), fields.at(4));
            entry.fps = fpsFrom(fields.at(4));
        }

        return entry;
    }

    // Reading a timecode back into milliseconds.
    //
    // The Library stores the length as the timecode it displays rather than as a
    // number, because that is what it has always stored and changing it would
    // mean rewriting every row. Sorting by length needs a number, so this reads
    // the string back.
    //
    // Frames are deliberately ignored. Without the rate they cannot be turned
    // into time, and a frame is under 40 ms - far below anything that decides an
    // ordering between two clips.
    inline qint64 msFromTimecode(const QString& timecode)
    {
        if (timecode.isEmpty())
            return UNKNOWN;

        // Drop-frame notation writes the last separator as a dot; the client
        // switches between the two on a setting, so both arrive here.
        QString normalised = timecode;
        normalised.replace('.', ':');

        const QStringList parts = normalised.split(':');
        if (parts.size() < 3)
            return UNKNOWN;

        bool hoursOk = false;
        bool minutesOk = false;
        bool secondsOk = false;

        const qint64 hours = parts.at(0).toLongLong(&hoursOk);
        const qint64 minutes = parts.at(1).toLongLong(&minutesOk);
        const qint64 seconds = parts.at(2).toLongLong(&secondsOk);

        if (!hoursOk || !minutesOk || !secondsOk)
            return UNKNOWN;
        if (hours < 0 || minutes < 0 || seconds < 0)
            return UNKNOWN;

        return ((hours * 60 + minutes) * 60 + seconds) * 1000;
    }

    // ---------------------------------------------------------------- sorting --

    enum class SortBy
    {
        Name,       // what it has always done
        Newest,     // the clip that just landed, which is the actual question
        Largest,
        Longest
    };

    // Stored in the database, so these strings cannot be renamed without a
    // migration.
    inline QString toKey(SortBy sort)
    {
        switch (sort)
        {
            case SortBy::Newest:  return "newest";
            case SortBy::Largest: return "largest";
            case SortBy::Longest: return "longest";
            default:              return "name";
        }
    }

    // An unrecognised key is Name, not an error. The value comes out of a
    // database that an older or newer build may have written.
    inline SortBy fromKey(const QString& key)
    {
        const QString value = key.trimmed().toLower();

        if (value == "newest")
            return SortBy::Newest;
        if (value == "largest")
            return SortBy::Largest;
        if (value == "longest")
            return SortBy::Longest;

        return SortBy::Name;
    }

    inline QString label(SortBy sort)
    {
        switch (sort)
        {
            case SortBy::Newest:  return "Newest first";
            case SortBy::Largest: return "Largest first";
            case SortBy::Longest: return "Longest first";
            default:              return "Name";
        }
    }

    // Ordering two rows. The three interesting parts:
    //
    //   Unknown sorts last, always - under every order and in both directions. A
    //   server that sends no size for stills would otherwise fill the top of a
    //   size-sorted list with them, which looks like a bug and hides the answer.
    //
    //   Ties fall back to the name, so the list has one stable order rather than
    //   whatever the database happened to return. Two clips of the same length
    //   swapping places between refreshes is the kind of thing that makes a panel
    //   feel broken.
    //
    //   Names compare case-insensitively, because a folder with Opener.mov and
    //   opener.mov in it should show them together.
    inline int compareNames(const QString& left, const QString& right)
    {
        const int result = QString::compare(left, right, Qt::CaseInsensitive);

        // Identical but for case still needs a stable answer.
        return result != 0 ? result : QString::compare(left, right, Qt::CaseSensitive);
    }

    inline bool lessThan(const Entry& left, const Entry& right, SortBy sort)
    {
        switch (sort)
        {
            case SortBy::Newest:
            {
                const bool leftHas = !left.timestamp.isEmpty();
                const bool rightHas = !right.timestamp.isEmpty();

                if (leftHas != rightHas)
                    return leftHas;
                if (leftHas && left.timestamp != right.timestamp)
                    return left.timestamp > right.timestamp;

                break;
            }
            case SortBy::Largest:
            {
                if (left.hasSize() != right.hasSize())
                    return left.hasSize();
                if (left.hasSize() && left.sizeBytes != right.sizeBytes)
                    return left.sizeBytes > right.sizeBytes;

                break;
            }
            case SortBy::Longest:
            {
                if (left.hasDuration() != right.hasDuration())
                    return left.hasDuration();
                if (left.hasDuration() && left.durationMs != right.durationMs)
                    return left.durationMs > right.durationMs;

                break;
            }
            default:
                break;
        }

        return compareNames(left.name, right.name) < 0;
    }

    // ---------------------------------------------------------------- display --

    // Sizes for a narrow column: three significant figures at most, and the unit
    // the number is actually in rather than everything in megabytes.
    inline QString formatSize(qint64 bytes)
    {
        if (bytes < 0)
            return QString();

        if (bytes < 1024)
            return QString("%1 B").arg(bytes);

        double value = static_cast<double>(bytes) / 1024.0;
        const char* units[] = { "KB", "MB", "GB", "TB" };
        int unit = 0;

        while (value >= 1024.0 && unit < 3)
        {
            value /= 1024.0;
            unit++;
        }

        return QString("%1 %2").arg(value, 0, 'f', value < 10.0 ? 1 : 0).arg(units[unit]);
    }

    // A date for a tooltip. Today's files show a time, older ones a date - the
    // useful distinction when the question is "did this arrive just now".
    inline QString formatDate(const QString& stamp)
    {
        const QDateTime when = dateFrom(stamp);
        if (!when.isValid())
            return QString();

        if (when.date() == QDate::currentDate())
            return when.toString("HH:mm");

        return when.toString("yyyy-MM-dd HH:mm");
    }
}
