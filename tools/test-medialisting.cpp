// Reading what a server says it has, and ordering it.
//
// CLS sends name, type, size, timestamp, frames and timebase on every line. The
// client parsed the last two into a timecode and threw the size and the date
// away - so the Library could sort by name and nothing else, while the two fields
// that answer "where is the clip that just landed" arrived on every refresh and
// were dropped.
//
// The parsing is the risky half. Names contain spaces and the quotes are the only
// thing separating them from the fields; trailing fields go missing; and
// CasparCG's own documentation contains a line whose timestamp is not a date. The
// sorting is the subtle half: a missing field has to sort somewhere deliberate,
// because a size-sorted list that opens with every still on the server looks like
// a bug rather than like missing data.

#include "../src/Common/MediaListing.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTextStream>

#include <algorithm>

static int failures = 0;
static int checks = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void sameNum(qint64 actual, qint64 wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void aRealLineIsReadWhole()
{
    // Straight from the CasparCG documentation.
    const MediaListing::Entry entry =
        MediaListing::parseLine("\"AMB\"  MOVIE  6445960 20121101160514 643 1/60");

    same(entry.name, "AMB", "the name");
    same(entry.type, "MOVIE", "the type");
    sameNum(entry.sizeBytes, 6445960, "the size in bytes, which used to be discarded");
    same(entry.timestamp, "20121101160514", "the timestamp, which used to be discarded");

    // 643 frames at 60 fps.
    sameNum(entry.durationMs, 10717, "the length in milliseconds");
    expectTrue(entry.isValid(), "and the row is usable");
}

static void aNameWithSpacesSurvives()
{
    // The reason the quotes are the delimiter and not the whitespace. Most real
    // media folders are full of these.
    const MediaListing::Entry entry =
        MediaListing::parseLine("\"OPENER FINAL v2\"  MOVIE  123456 20240301093000 250 1/25");

    same(entry.name, "OPENER FINAL v2", "a name with spaces is kept whole");
    same(entry.type, "MOVIE", "and the fields after it still line up");
    sameNum(entry.sizeBytes, 123456, "including the size");
}

static void aNameInAFolderIsNormalised()
{
    const MediaListing::Entry entry =
        MediaListing::parseLine("\"SHOW\\STING\"  MOVIE  100 20240301093000 25 1/25");

    same(entry.name, "SHOW/STING", "backslashes become the separator the rest of the client uses");
}

static void theTimebaseIsReadTheWayFfmpegWritesIt()
{
    // 100/2997 is 29.97 fps, not 0.03. Getting the fraction upside down would
    // make every NTSC clip a thousand times too long, and it would sort first.
    const MediaListing::Entry ntsc =
        MediaListing::parseLine("\"HOOLOOVOO\"  MOVIE  1111111 20121101150514 2997 100/2997");

    // 2997 frames at 29.97 fps is 100 seconds.
    sameNum(ntsc.durationMs, 100000, "29.97 fps is read as 29.97, not as its reciprocal");

    const MediaListing::Entry pal =
        MediaListing::parseLine("\"CG1080I50\"  MOVIE  6159792 20121101150514 250 1/25");
    sameNum(pal.durationMs, 10000, "and 25 fps gives ten seconds for 250 frames");
}

static void theRateComesOutWithTheLength()
{
    // The Library still shows a timecode, and building one needs the rate as well
    // as the length. The rate is carried on the parsed row for exactly that
    // reason: the first version of this change split the line a second time to
    // get it, with the field indices copied from code whose split kept the empty
    // element the double space produces. They were off by one, and every timecode
    // in the Library would have quietly disappeared.
    const MediaListing::Entry pal =
        MediaListing::parseLine("\"CG1080I50\"  MOVIE  6159792 20121101150514 250 1/25");
    expectTrue(qAbs(pal.fps - 25.0) < 0.001, "25 fps comes out of 1/25");

    const MediaListing::Entry ntsc =
        MediaListing::parseLine("\"NTSC\"  MOVIE  100 20121101150514 2997 100/2997");
    expectTrue(qAbs(ntsc.fps - 29.97) < 0.01, "and 29.97 out of 100/2997");

    const MediaListing::Entry sixty =
        MediaListing::parseLine("\"AMB\"  MOVIE  6445960 20121101160514 643 1/60");
    expectTrue(qAbs(sixty.fps - 60.0) < 0.001, "and 60 out of 1/60");

    // A row with no rate must say so rather than report zero as a rate, because
    // the caller divides by it.
    const MediaListing::Entry still = MediaListing::parseLine("\"LOGO\"  STILL  45210 20240101120000");
    expectTrue(still.fps == 0.0, "a row with no rate reports none");

    const MediaListing::Entry junk =
        MediaListing::parseLine("\"X\"  MOVIE  1 20240101120000 25 notafraction");
    expectTrue(junk.fps == 0.0, "and neither does a timebase that is not a fraction");
    expectTrue(!junk.hasDuration(), "which also means no length");

    // Division by zero in the fraction itself.
    expectTrue(MediaListing::fpsFrom("0/25") == 0.0, "a zero numerator is not a rate");
    expectTrue(MediaListing::fpsFrom("1/0") == 0.0, "and neither is a zero denominator");
    expectTrue(MediaListing::fpsFrom("") == 0.0, "and neither is nothing at all");

    // The rate and the length have to agree, or a timecode built from them is
    // wrong in a way nothing else would catch.
    expectTrue(qAbs(pal.durationMs / 1000.0 * pal.fps - 250.0) < 0.5,
               "the length and the rate multiply back to the frame count");
}

static void aMissingFieldIsMissingRatherThanZero()
{
    // A still has no frame count. A clip of unknown length is not a clip of
    // length zero, and the difference decides where it sorts.
    const MediaListing::Entry still = MediaListing::parseLine("\"LOGO\"  STILL  45210 20240101120000");

    same(still.name, "LOGO", "a short line still gives a name");
    same(still.type, "STILL", "and a type");
    sameNum(still.sizeBytes, 45210, "and a size");
    expectTrue(!still.hasDuration(), "but no length, rather than a length of zero");
    sameNum(still.durationMs, MediaListing::UNKNOWN, "which is unknown, not 0");

    const MediaListing::Entry bare = MediaListing::parseLine("\"THING\"  STILL");
    expectTrue(bare.isValid(), "a name and a type alone is still a row");
    expectTrue(!bare.hasSize(), "with an unknown size");
    expectTrue(!bare.hasDuration(), "and an unknown length");
}

static void rubbishIsRefusedRatherThanGuessedAt()
{
    // This exact line is in CasparCG's own documentation. 22222222222222 is
    // fourteen digits and is not a date.
    const MediaListing::Entry entry =
        MediaListing::parseLine("\"HOOLOOVOO\"  MOVIE  1111111 22222222222222 333 100/2997");

    same(entry.name, "HOOLOOVOO", "the name is still read");
    sameNum(entry.sizeBytes, 1111111, "and the size");
    expectTrue(entry.timestamp.isEmpty(),
               "but a timestamp that is not a date is no timestamp at all");

    // Which matters, because a bad date that parsed would sort to one end of the
    // list and stay there.
    expectTrue(!MediaListing::dateFrom("22222222222222").isValid(), "and it is not a valid date");
    expectTrue(!MediaListing::dateFrom("2024030109300").isValid(), "nor is a thirteen-digit one");
    expectTrue(!MediaListing::dateFrom("2024-03-01T09:30").isValid(), "nor an ISO one");
    expectTrue(MediaListing::dateFrom("20240301093000").isValid(), "a real one is");
}

static void aLineThatIsNotAListingIsNotARow()
{
    expectTrue(!MediaListing::parseLine("").isValid(), "an empty line is not a row");
    expectTrue(!MediaListing::parseLine("201 CLS OK").isValid(), "and neither is a response header");
    expectTrue(!MediaListing::parseLine("\"unterminated MOVIE 1").isValid(),
               "and neither is a name with no closing quote");
    expectTrue(!MediaListing::parseLine("\"\"  MOVIE  1 20240101120000").isValid(),
               "and an empty name is not a row either");
}

static void aNegativeOrJunkSizeIsUnknown()
{
    const MediaListing::Entry junk =
        MediaListing::parseLine("\"X\"  MOVIE  notanumber 20240101120000 25 1/25");
    expectTrue(!junk.hasSize(), "a size that is not a number is unknown");
    sameNum(junk.durationMs, 1000, "and the fields after it are still read");

    const MediaListing::Entry negative =
        MediaListing::parseLine("\"X\"  MOVIE  -5 20240101120000 25 1/25");
    expectTrue(!negative.hasSize(), "and so is a negative one");
}

static void aTimecodeReadsBackIntoMilliseconds()
{
    // The Library stores a length as the timecode it shows, so sorting by length
    // has to read it back.
    sameNum(MediaListing::msFromTimecode("00:00:10:00"), 10000, "ten seconds");
    sameNum(MediaListing::msFromTimecode("00:01:30:12"), 90000, "a minute and a half");
    sameNum(MediaListing::msFromTimecode("01:00:00:00"), 3600000, "an hour");
    sameNum(MediaListing::msFromTimecode("00:00:00:00"), 0, "and nothing is zero, not unknown");

    // Drop-frame notation writes the last separator as a dot, and the client
    // switches between the two on a setting - so both forms arrive here.
    sameNum(MediaListing::msFromTimecode("00:01:30.12"), 90000, "drop-frame notation reads the same");

    // Nothing at all is unknown, which is what puts it at the end of the list
    // rather than at the top.
    sameNum(MediaListing::msFromTimecode(""), MediaListing::UNKNOWN, "an empty timecode is unknown");
    sameNum(MediaListing::msFromTimecode("--"), MediaListing::UNKNOWN, "and so is rubbish");
    sameNum(MediaListing::msFromTimecode("12:34"), MediaListing::UNKNOWN, "and so is a partial one");
    sameNum(MediaListing::msFromTimecode("aa:bb:cc:dd"), MediaListing::UNKNOWN, "and so are letters");

    // Longer really is longer, which is the only property the sort depends on.
    expectTrue(MediaListing::msFromTimecode("00:02:00:00") > MediaListing::msFromTimecode("00:01:59:24"),
               "two minutes is longer than one fifty-nine and change");
}

// ------------------------------------------------------------------ sorting --

static QList<MediaListing::Entry> ordered(QList<MediaListing::Entry> entries, MediaListing::SortBy sort)
{
    std::stable_sort(entries.begin(), entries.end(),
                     [sort](const MediaListing::Entry& l, const MediaListing::Entry& r) {
                         return MediaListing::lessThan(l, r, sort);
                     });

    return entries;
}

static MediaListing::Entry make(const QString& name, qint64 size, const QString& stamp, qint64 durationMs)
{
    MediaListing::Entry entry;
    entry.name = name;
    entry.type = "MOVIE";
    entry.sizeBytes = size;
    entry.timestamp = stamp;
    entry.durationMs = durationMs;

    return entry;
}

static void nameIsWhatItAlwaysWas()
{
    QList<MediaListing::Entry> entries;
    entries << make("zulu", 1, "20240101120000", 1000)
            << make("alpha", 9, "20240301120000", 9000)
            << make("Mike", 5, "20240201120000", 5000);

    const QList<MediaListing::Entry> sorted = ordered(entries, MediaListing::SortBy::Name);

    same(sorted.at(0).name, "alpha", "name order is alphabetical");
    same(sorted.at(1).name, "Mike", "and case does not split the list in two");
    same(sorted.at(2).name, "zulu", "as it would if it compared by byte");
}

static void newestPutsTheClipThatJustLandedFirst()
{
    // The actual question an operator has, and the one the Library could not
    // answer at all.
    QList<MediaListing::Entry> entries;
    entries << make("old", 1, "20200101120000", 1000)
            << make("newest", 1, "20240601093000", 1000)
            << make("middle", 1, "20220301120000", 1000);

    const QList<MediaListing::Entry> sorted = ordered(entries, MediaListing::SortBy::Newest);

    same(sorted.at(0).name, "newest", "the most recent file is first");
    same(sorted.at(1).name, "middle", "then the next");
    same(sorted.at(2).name, "old", "then the oldest");
}

static void largestAndLongestPutTheBigOnesFirst()
{
    QList<MediaListing::Entry> entries;
    entries << make("small", 100, "20240101120000", 1000)
            << make("huge", 900000, "20240101120000", 500)
            << make("medium", 5000, "20240101120000", 90000);

    const QList<MediaListing::Entry> bySize = ordered(entries, MediaListing::SortBy::Largest);
    same(bySize.at(0).name, "huge", "the largest file is first");
    same(bySize.at(2).name, "small", "and the smallest last");

    const QList<MediaListing::Entry> byLength = ordered(entries, MediaListing::SortBy::Longest);
    same(byLength.at(0).name, "medium", "the longest item is first");
    same(byLength.at(2).name, "huge", "regardless of how big its file is");
}

static void unknownAlwaysSortsLast()
{
    // The one that makes the feature usable rather than annoying. A server that
    // sends no size for stills would otherwise fill the top of a size-sorted list
    // with them, and the operator would conclude the sort is broken.
    QList<MediaListing::Entry> entries;
    entries << make("nosize", MediaListing::UNKNOWN, "20240101120000", 1000)
            << make("tiny", 1, "20240101120000", 1000)
            << make("big", 900000, "20240101120000", 1000);

    const QList<MediaListing::Entry> bySize = ordered(entries, MediaListing::SortBy::Largest);
    same(bySize.at(0).name, "big", "the known sizes come first");
    same(bySize.at(1).name, "tiny", "in order");
    same(bySize.at(2).name, "nosize", "and the unknown one is last, not first");

    QList<MediaListing::Entry> undated;
    undated << make("nodate", 1, "", 1000)
            << make("dated", 1, "20200101120000", 1000);
    same(ordered(undated, MediaListing::SortBy::Newest).at(1).name, "nodate",
         "and an undated file sorts after every dated one, however old");

    QList<MediaListing::Entry> unmeasured;
    unmeasured << make("nolength", 1, "20240101120000", MediaListing::UNKNOWN)
               << make("brief", 1, "20240101120000", 1);
    same(ordered(unmeasured, MediaListing::SortBy::Longest).at(1).name, "nolength",
         "and one of unknown length after the briefest known one");
}

static void tiesFallBackToTheName()
{
    // Without this the order of equal rows is whatever the database returned, and
    // two clips swapping places between refreshes makes a panel feel broken.
    QList<MediaListing::Entry> entries;
    entries << make("charlie", 500, "20240101120000", 1000)
            << make("alpha", 500, "20240101120000", 1000)
            << make("bravo", 500, "20240101120000", 1000);

    for (int sort = 0; sort < 4; sort++)
    {
        const MediaListing::SortBy by = static_cast<MediaListing::SortBy>(sort);
        const QList<MediaListing::Entry> sorted = ordered(entries, by);

        same(sorted.at(0).name, "alpha",
             QString("under %1, equal rows fall back to the name").arg(MediaListing::label(by)));
        same(sorted.at(2).name, "charlie",
             QString("under %1, all the way down").arg(MediaListing::label(by)));
    }
}

static void everyUnknownIsAStableOrderToo()
{
    // Two rows that are both unknown still need one answer, or they swap.
    QList<MediaListing::Entry> entries;
    entries << make("zulu", MediaListing::UNKNOWN, "", MediaListing::UNKNOWN)
            << make("alpha", MediaListing::UNKNOWN, "", MediaListing::UNKNOWN);

    same(ordered(entries, MediaListing::SortBy::Largest).at(0).name, "alpha",
         "two unknowns are still ordered by name");
    same(ordered(entries, MediaListing::SortBy::Newest).at(0).name, "alpha",
         "under every sort");
}

// ------------------------------------------------------------ the stored key --

static void theStoredKeyRoundTrips()
{
    // The key goes in the database, so it has to survive being written and read.
    const MediaListing::SortBy all[] = {
        MediaListing::SortBy::Name, MediaListing::SortBy::Newest,
        MediaListing::SortBy::Largest, MediaListing::SortBy::Longest
    };

    for (const MediaListing::SortBy sort : all)
    {
        expectTrue(MediaListing::fromKey(MediaListing::toKey(sort)) == sort,
                   QString("\"%1\" survives the round trip").arg(MediaListing::toKey(sort)));
        expectTrue(!MediaListing::label(sort).isEmpty(),
                   QString("\"%1\" has something to show in the menu").arg(MediaListing::toKey(sort)));
    }

    // A value written by a newer build, or by hand, must not leave the Library
    // sorted by nothing.
    expectTrue(MediaListing::fromKey("rating") == MediaListing::SortBy::Name,
               "an unrecognised key falls back to name");
    expectTrue(MediaListing::fromKey("") == MediaListing::SortBy::Name,
               "and so does an empty one");
    expectTrue(MediaListing::fromKey("  NEWEST  ") == MediaListing::SortBy::Newest,
               "and a key is not case or whitespace sensitive");
}

static void sizesReadAsSizes()
{
    same(MediaListing::formatSize(0), "0 B", "zero bytes");
    same(MediaListing::formatSize(512), "512 B", "bytes stay bytes");
    same(MediaListing::formatSize(6445960), "6.1 MB", "the AMB clip from the documentation");
    same(MediaListing::formatSize(1024LL * 1024 * 1024 * 3), "3.0 GB", "gigabytes");
    same(MediaListing::formatSize(MediaListing::UNKNOWN), "",
         "and an unknown size shows nothing rather than a wrong number");

    // A column three characters wide cannot show 1234.5678 MB.
    expectTrue(MediaListing::formatSize(999999999999LL).length() <= 8,
               "even a very large size fits a narrow column");
}

static void datesReadAsDates()
{
    same(MediaListing::formatDate("20240301093000"), "2024-03-01 09:30", "an older file shows its date");
    same(MediaListing::formatDate("22222222222222"), "",
         "and rubbish shows nothing rather than a wrong date");
    same(MediaListing::formatDate(""), "", "and so does an absent one");

    // Today's files show a time, which is the distinction that matters when the
    // question is whether something just arrived.
    const QString today = QDate::currentDate().toString("yyyyMMdd") + "143000";
    same(MediaListing::formatDate(today), "14:30", "a file from today shows the time");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Media listing\n";

    aRealLineIsReadWhole();
    aNameWithSpacesSurvives();
    aNameInAFolderIsNormalised();
    theTimebaseIsReadTheWayFfmpegWritesIt();
    theRateComesOutWithTheLength();
    aMissingFieldIsMissingRatherThanZero();
    rubbishIsRefusedRatherThanGuessedAt();
    aLineThatIsNotAListingIsNotARow();
    aNegativeOrJunkSizeIsUnknown();

    aTimecodeReadsBackIntoMilliseconds();

    nameIsWhatItAlwaysWas();
    newestPutsTheClipThatJustLandedFirst();
    largestAndLongestPutTheBigOnesFirst();
    unknownAlwaysSortsLast();
    tiesFallBackToTheName();
    everyUnknownIsAStableOrderToo();

    theStoredKeyRoundTrips();
    sizesReadAsSizes();
    datesReadAsDates();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
