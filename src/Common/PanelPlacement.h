#pragma once

// Whether a panel is placed in the layout at all.
//
// Panels are optional. A layout holds up to four columns, each a comma-separated
// list of panel ids, and anything not named in one of them is not on screen and
// has no way of getting there without the layout changing.
//
// This matters because the client builds every panel whether or not the layout
// asks for it, and some of them then work continuously. The Activity panel
// rebuilds a row on every playback-progress event — which arrive per playing
// layer at the OSC polling rate, five to twenty a second during a show — and
// sweeps for stale rows twice a second, forever, for a panel that may not be
// anywhere.
//
// So this is not about hiding: a hidden widget still receives the signal and
// still does the work. It is about not doing the work at all.
//
// Header-only so the rule can be compiled and tested without a database.

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace PanelPlacement
{
    // The four column values from the layout, in any order. Each is what the
    // layout editor writes: "Library,Preview" and so on. Empty entries are
    // ordinary — most layouts do not use all four columns.
    inline bool isPlaced(const QString& panelId, const QStringList& columnContents)
    {
        const QString id = panelId.trimmed();
        if (id.isEmpty())
            return false;

        for (const QString& column : columnContents)
        {
            const QStringList ids = column.split(',', Qt::SkipEmptyParts);

            for (const QString& candidate : ids)
            {
                // Ids are written by the layout editor and read back here, so a
                // stray space around a comma is the operator's editing rather
                // than corruption.
                if (candidate.trimmed().compare(id, Qt::CaseInsensitive) == 0)
                    return true;
            }
        }

        return false;
    }

    // The configuration keys holding those columns, for the layout in use.
    // Simple Mode keeps its own arrangement, so a panel can be placed in one and
    // not the other and the answer has to follow whichever is active.
    inline QStringList columnKeys(bool simpleModeActive)
    {
        const QString prefix = simpleModeActive ? QString("Simple") : QString();

        return QStringList()
            << prefix + "LayoutPanel1"
            << prefix + "LayoutPanel2"
            << prefix + "LayoutPanel3"
            << prefix + "LayoutPanel4";
    }
}
