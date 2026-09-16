// The Shotbox panel's rules: what goes in, how many, where a moved row lands,
// and what the operator is told when a drop does not all fit.

#include "../src/Common/ShotboxRules.h"

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

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Shotbox\n";

    using namespace ShotboxRules;

    expectTrue(accepts("MOVIE") && accepts("TEMPLATE") && accepts("STILL") && accepts("AUDIO"), "playable items go in");
    expectTrue(accepts("CUSTOMCOMMAND") && accepts("HTTPGET"), "so do command items");
    expectTrue(!accepts("GROUP"), "a group does not: it fires its children from the rundown");
    expectTrue(!accepts("AUTOPLAYGATEWAY") && !accepts("FOCUSGATEWAY") && !accepts("COMMANDGATEWAY"), "nor a gateway");
    expectTrue(!accepts(""), "nor something with no type");

    expectTrue(room(0) == MAX_ROWS && MAX_ROWS == 8, "an empty Shotbox takes eight");
    expectTrue(room(6) == 2 && room(8) == 0 && room(11) == 0, "a full one takes none, and never a negative number");

    expectTrue(moveTarget(0, 3, 5) == 3, "a row dropped lower moves there");
    expectTrue(moveTarget(4, 0, 5) == 0, "and higher");
    expectTrue(moveTarget(2, 99, 5) == 4, "dropped below the last row, it goes to the end");
    expectTrue(moveTarget(2, 2, 5) == -1, "dropped on itself, nothing moves");
    expectTrue(moveTarget(0, 0, 1) == -1 && moveTarget(5, 1, 5) == -1, "nor with one row, or a row that is not there");

    expectTrue(dropNotice(3, 3, 0).isEmpty(), "everything landed: nothing to say");
    expectTrue(dropNotice(1, 0, 1) == "A group or gateway cannot go in the Shotbox", "one refused type is named");
    expectTrue(dropNotice(4, 2, 0) == "The Shotbox holds 8 items, so 2 were not added", "a full Shotbox says how many did not fit");
    expectTrue(dropNotice(4, 1, 2) == "2 groups or gateways cannot go in the Shotbox; the Shotbox holds 8 items, so 1 was not added",
               "both reasons together");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
