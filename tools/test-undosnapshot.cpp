// An undo step gives back exactly the rundown it took.
//
// Undo snapshots are stored compressed (UndoSnapshot.h). A round trip that lost
// or changed anything would be an undo that quietly restores a different
// rundown, so the round trip is checked on the text a rundown actually holds:
// names in other scripts, XML escapes, line endings, and nothing at all.

#include "../src/Common/UndoSnapshot.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void roundTrip(const QString& xml, const QString& what)
{
    expectTrue(UndoSnapshot::unpack(UndoSnapshot::pack(xml)) == xml, what);
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Undo snapshots\n";

    roundTrip(QString(), "the empty rundown");
    roundTrip("<items></items>", "a rundown with no items");
    roundTrip(QString::fromUtf8("<items><item><label>S\xC3\xA9ville \xE2\x80\x94 \xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x8E\xAC</label></item></items>"),
              "names with accents, a dash, CJK and an emoji");
    roundTrip("<items>\r\n<item><templatedata>&lt;b&gt; &amp; &quot;x&quot;</templatedata></item>\r\n</items>\n",
              "escapes and mixed line endings");

    // A large, repetitive rundown, like a real one.
    QString big = "<items>";
    for (int i = 0; i < 5000; i++)
        big += QString("<item><type>MOVIE</type><label>CLIP_%1</label><devicename>Server A</devicename>"
                       "<channel>1</channel><videolayer>10</videolayer><delay>0</delay></item>").arg(i);
    big += "</items>";

    const QByteArray packed = UndoSnapshot::pack(big);
    expectTrue(UndoSnapshot::unpack(packed) == big, "a 5,000-item rundown comes back exactly");
    out << QString("  5,000 items: %1 KB held, %2 KB stored\n").arg(big.size() * 2 / 1024).arg(packed.size() / 1024);
    expectTrue(packed.size() * 10 < big.size() * 2, "and is stored in under a tenth of the memory");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
