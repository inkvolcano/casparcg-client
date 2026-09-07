#pragma once

#include <QtCore/qglobal.h>

// Keeping a column of panels inside the screen.
//
// A panel with a fixed height cannot shrink, so its height becomes part of a floor
// the window can never go under. Qt settles a layout minimum against an explicit
// maximum by honouring the minimum, so once the stored heights in one column add up
// past the screen, setMaximumSize on the window achieves nothing: the window is
// taller than the display and cannot be dragged back.
//
// That is the bug. A panel dragged tall on a large monitor keeps that height in the
// database, and opening the client on a laptop leaves a window that will not fit and
// will not shrink.
//
// The fix is two steps, and it has to be two. Deciding each panel on its own does
// not work: the first one takes what it wants, and everything after it is left with
// a floor apiece, which still adds up past the screen. So the column is measured
// first and every panel is then reduced by the same proportion, which keeps their
// relative sizes and makes the total fit.
//
// Header-only so it can be tested without linking the application.

namespace PanelFit
{
    // What a panel needs to be worth showing at all. Below this it is better to let
    // a window be visibly too tall than to crush every panel to a title bar, which
    // would look like a different and more baffling fault.
    const int FLOOR = 80;

    // Room the window needs for itself: menu, status bar, borders, the rundown.
    const int CHROME = 120;
}

/**
 * How much of what a column asked for it can actually have, as a factor.
 *
 * 1.0 means everything fits and nothing should be touched, which is the normal case
 * and the one worth keeping fast and obvious.
 */
inline double panelColumnScale(int wantedTotal, int availableHeight)
{
    if (availableHeight <= 0 || wantedTotal <= 0)
        return 1.0;                 // nothing to measure against, so do not guess

    int budget = availableHeight - PanelFit::CHROME;
    if (budget < PanelFit::FLOOR)
        budget = PanelFit::FLOOR;

    if (wantedTotal <= budget)
        return 1.0;

    return static_cast<double>(budget) / static_cast<double>(wantedTotal);
}

/** One panel's height under that factor, never below the floor. */
inline int fitPanelHeight(int stored, double scale)
{
    if (stored <= 0)
        return stored;              // nothing stored: the caller has its own default

    if (scale >= 1.0)
        return stored;

    int scaled = static_cast<int>(stored * scale);
    return qMax(PanelFit::FLOOR, qMin(stored, scaled));
}
