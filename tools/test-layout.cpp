// Keeping a column of panels inside the screen.
//
// A panel with a fixed height cannot shrink, so its height joins a floor the window
// can never go under. Qt settles a layout minimum against an explicit maximum by
// honouring the minimum, which is why setMaximumSize on the window does nothing once
// a column of stored heights adds up past the display: the window is taller than the
// screen and cannot be dragged back.
//
// The numbers below are from a real database, not invented. An Inspector stored at
// 962 px with a Clock, a Server Status and an Activity panel in the same column,
// opened on a 1080p screen.
//
// The first version of this rule decided each panel on its own and passed every case
// except that one. It gave the Inspector what it asked for and left the other three a
// floor apiece, which still came to 1160 px on a 1040 px screen. That failure is why
// the rule measures the whole column before handing any of it out.

#include "../src/Common/PanelFit.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void same(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << wanted << ", got " << actual << ")\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

/** Lay out a column the way the builder does, and return what it comes to. */
static int columnTotal(const int* wanted, int count, int screen)
{
    int asked = 0;
    for (int i = 0; i < count; i++)
        asked += wanted[i];

    double scale = panelColumnScale(asked, screen);

    int total = 0;
    for (int i = 0; i < count; i++)
        total += fitPanelHeight(wanted[i], scale);

    return total;
}

int main()
{
    QTextStream out(stdout);

    const int LAPTOP = 1040;     // 1080p with a taskbar
    const int BIG = 1600;      // a 1600p panel: 1415 px of stored heights fits inside it

    out << "Nothing is touched when it already fits\n";

    same(fitPanelHeight(150, panelColumnScale(150, LAPTOP)), 150, "one small panel");
    same(fitPanelHeight(498, panelColumnScale(498, LAPTOP)), 498, "one large panel");

    {
        const int column[] = { 150, 153, 150, 201 };   // 654, comfortably inside
        same(columnTotal(column, 4, LAPTOP), 654, "a column that fits keeps every pixel");
    }

    expectTrue(panelColumnScale(654, LAPTOP) == 1.0, "and is not scaled at all");

    out << "\nThe column from the real database\n";

    {
        // Inspector, Clock, ServerStatus, Activity. 1415 px asked for.
        const int column[] = { 962, 150, 153, 150 };
        int total = columnTotal(column, 4, LAPTOP);

        expectTrue(962 + 150 + 153 + 150 > LAPTOP, "the stored heights really do not fit");
        expectTrue(total <= LAPTOP, QString("the laid-out column fits: %1 <= %2").arg(total).arg(LAPTOP));

        // The version that decided each panel on its own produced 1160 here.
        expectTrue(total < 1160, QString("and beats deciding each panel alone: %1 < 1160").arg(total));
    }

    out << "\nThe two columns from that machine's own layout\n";

    {
        // LayoutPanel3 is the Inspector on its own, resizable, stored at 962. On a
        // 1080p screen that one panel plus the window's own chrome already does not
        // fit, which is the whole bug in a single column.
        const int inspectorColumn[] = { 962 };
        int total = columnTotal(inspectorColumn, 1, LAPTOP);

        expectTrue(962 + PanelFit::CHROME > LAPTOP, "the Inspector alone really does not fit");
        expectTrue(total <= LAPTOP - PanelFit::CHROME,
                   QString("and is brought inside: %1").arg(total));
        expectTrue(total > 900, QString("giving up almost nothing: %1 of 962").arg(total));
    }

    {
        // LayoutPanel4: Sheets, Clock, Performance, Server Status. 946 px, which also
        // does not fit once the window's chrome is counted.
        const int sideColumn[] = { 495, 150, 148, 153 };
        int total = columnTotal(sideColumn, 4, LAPTOP);

        expectTrue(946 + PanelFit::CHROME > LAPTOP, "that column does not fit either");
        expectTrue(total <= LAPTOP - PanelFit::CHROME, QString("and is brought inside: %1").arg(total));

        // The readability question. Every panel has a height it was designed for, and
        // squeezing one below that is how a fix for a layout bug becomes a worse bug.
        double scale = panelColumnScale(946, LAPTOP);
        expectTrue(fitPanelHeight(495, scale) >= 320, "Sheets stays above its designed 320");
        expectTrue(fitPanelHeight(150, scale) >= 58, "the Clock stays above its designed 58");
        expectTrue(fitPanelHeight(148, scale) >= 110, "Performance stays above its designed 110");

        expectTrue(total > 900, QString("and the column gives up under 5 per cent: %1 of 946").arg(total));
    }

    out << "\nEverything shrinks together\n";

    {
        // Relative sizes are what make a layout recognisable, so the big panel stays
        // the big one rather than being singled out.
        double scale = panelColumnScale(1415, LAPTOP);
        int inspector = fitPanelHeight(962, scale);
        int clock = fitPanelHeight(150, scale);

        expectTrue(inspector > clock * 3, "the Inspector is still much the largest");
        expectTrue(clock >= PanelFit::FLOOR, "and the small ones are not crushed below the floor");
        expectTrue(inspector < 962, "while the one that caused the problem did give something up");
    }

    out << "\nNothing is reduced to nothing\n";

    same(fitPanelHeight(500, 0.01), PanelFit::FLOOR, "a savage factor still leaves the floor");
    same(fitPanelHeight(40, 0.5), PanelFit::FLOOR, "a panel under the floor is raised to it, not halved");
    same(fitPanelHeight(0, 0.5), 0, "nothing stored stays nothing stored");

    out << "\nWhen there is nothing to measure against\n";

    expectTrue(panelColumnScale(1415, 0) == 1.0, "an unknown screen height changes nothing");
    expectTrue(panelColumnScale(1415, -1) == 1.0, "nor does a nonsense one");
    expectTrue(panelColumnScale(0, LAPTOP) == 1.0, "nor does a column that wants nothing");
    same(fitPanelHeight(962, 1.0), 962, "a factor of one is a no-op");

    out << "\nA bigger screen gives more back\n";

    {
        const int column[] = { 962, 150, 153, 150 };
        int onLaptop = columnTotal(column, 4, LAPTOP);
        int onBig = columnTotal(column, 4, BIG);

        expectTrue(onBig >= onLaptop, "a larger screen never allows less");
        same(onBig, 1415, "and a big enough one leaves the stored heights alone");
    }

    out << "\nA column of many panels still fits\n";

    {
        // Ten panels, each modest, together far too much. The case that would break a
        // rule which reserved a floor per panel instead of scaling.
        const int column[] = { 300, 300, 300, 300, 300, 300, 300, 300, 300, 300 };
        int total = columnTotal(column, 10, LAPTOP);
        expectTrue(total <= LAPTOP,
                   QString("ten panels wanting 3000 px fit in %1: got %2").arg(LAPTOP).arg(total));
    }

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
