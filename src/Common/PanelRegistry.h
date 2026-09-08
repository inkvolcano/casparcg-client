#pragma once

// Every panel the client has, declared once.
//
// Adding a panel used to mean editing six shared files and getting fourteen
// scattered mentions right: the layout editor's id list, its display-name map,
// the default height, the compact height, the widget lookup, the Panel Sizing
// row, its default size mode, and the set of keys a named layout saves. Miss one
// and the panel half-works — it places but does not size, or it sizes but is not
// saved in a named layout.
//
// That was not a hypothetical. The iNews panel could be placed in the layout
// editor, had no Panel Sizing row, and was not saved in a named layout, because
// its id was in one hand-copied list and not the other two. Nobody noticed
// because a panel almost nobody uses failing to remember its height is not the
// sort of thing that gets reported.
//
// So the lists are gone and this is the list. Everything that used to keep its
// own copy now reads this one, which means a panel that is here is placeable,
// sizeable, and saved — and a panel that is not here does not half-exist.
//
// One thing still lives outside: MainWindow::widgetById, which has to map an id
// to an actual member and cannot be written generically. That is the one place
// left to remember, and it is one rather than six.

#include "Global.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace PanelRegistry
{
    struct Entry
    {
        QString id;
        QString displayName;

        // 0 means the panel has no fixed default and takes what the column gives
        // it. The compact height is what a collapsed panel shrinks to; 25 is the
        // tab header on its own, which is the usual answer.
        int defaultHeight = 0;
        int compactHeight = 25;

        // What the Panel Sizing list starts it at: "fixed", "resizable" or
        // "expanding".
        QString defaultSizeMode = "fixed";

        // Kept for the people already using it, not offered to anyone else. A
        // legacy panel stays registered - it sizes, it is saved by a named
        // layout, and a layout that already places it is untouched - but it is
        // not in the list of panels you can add unless legacy panels are turned
        // on. Hiding it is reversible; removing it would not be.
        bool legacy = false;
    };

    // Built once. The lookups below run inside the layout rebuild, once per panel
    // per column, so rebuilding this list on every call would turn a constant into
    // a loop nobody can see.
    inline const QList<Entry>& all()
    {
        static const QList<Entry> registered = []() {
            QList<Entry> panels;

            panels.append({ "AudioLevels", "Audio Levels",
                            ::Panel::DEFAULT_AUDIOLEVELS_HEIGHT, ::Panel::COMPACT_AUDIOLEVELS_HEIGHT, "fixed" });
            panels.append({ "Preview", "Preview",
                            ::Panel::DEFAULT_PREVIEW_HEIGHT, ::Panel::COMPACT_PREVIEW_HEIGHT, "resizable" });
            panels.append({ "Library", "Library", 0, 25, "expanding" });
            // 86 lines of countdown LCD inherited at the fork and never touched,
            // named after a product most people running this client do not have.
            panels.append({ "Duration", "iNews", 0, 25, "fixed", true });
            panels.append({ "StatusBar", "Status Bar", 0, 25, "fixed" });
            panels.append({ "Clock", "Clock",
                            ::Panel::DEFAULT_CLOCK_HEIGHT, ::Panel::COMPACT_CLOCK_HEIGHT, "fixed" });
            panels.append({ "ServerStatus", "Server Status", 0, 25, "fixed" });
            panels.append({ "Activity", "Activity", 0, 25, "expanding" });
            panels.append({ "TriggerBanks", "Trigger Banks", 0, 25, "fixed" });
            panels.append({ "Live", "Live",
                            ::Panel::DEFAULT_LIVE_HEIGHT, ::Panel::COMPACT_LIVE_HEIGHT, "resizable" });

            // NDI shares the Live default height and has always collapsed to a bare
            // tab header rather than to a compact height of its own.
            panels.append({ "NDI", "NDI", ::Panel::DEFAULT_LIVE_HEIGHT, 25, "resizable" });

            panels.append({ "Performance", "Performance",
                            ::Panel::DEFAULT_PERFORMANCE_HEIGHT, ::Panel::COMPACT_PERFORMANCE_HEIGHT, "fixed" });
            panels.append({ "HttpLog", "Http Log",
                            ::Panel::DEFAULT_HTTPLOG_HEIGHT, ::Panel::COMPACT_HTTPLOG_HEIGHT, "fixed" });
            panels.append({ "Sheets", "Google Sheets",
                            ::Panel::DEFAULT_SHEETS_HEIGHT, ::Panel::COMPACT_SHEETS_HEIGHT, "resizable" });
            panels.append({ "Inspector", "Inspector", 0, 25, "expanding" });
            panels.append({ "SimpleInspector", "Simple Inspector",
                            ::Panel::DEFAULT_SIMPLE_INSPECTOR_HEIGHT, 25, "resizable" });

            return panels;
        }();

        return registered;
    }

    inline QStringList ids()
    {
        QStringList list;
        for (const Entry& panel : all())
            list.append(panel.id);

        return list;
    }

    // What the layout editor offers. Everything else - what a named layout saves,
    // what has a height, what has a sizing mode - still uses ids(), because a
    // panel somebody already placed has to keep working whether or not it is
    // still being offered.
    inline QStringList offeredIds(bool includeLegacy)
    {
        QStringList list;
        for (const Entry& panel : all())
        {
            if (panel.legacy && !includeLegacy)
                continue;

            list.append(panel.id);
        }

        return list;
    }

    inline bool isLegacy(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return panel.legacy;
        }

        return false;
    }

    inline bool contains(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return true;
        }

        return false;
    }

    // The display name, falling back to the id. A panel added to the registry
    // without a name is still placeable rather than appearing as a blank row.
    inline QString displayName(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return panel.displayName.isEmpty() ? panel.id : panel.displayName;
        }

        return id;
    }

    inline int defaultHeight(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return panel.defaultHeight;
        }

        return 0;
    }

    inline int compactHeight(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return panel.compactHeight;
        }

        // Just the tab header, which is what an unknown panel collapsing to
        // nothing would look wrong doing.
        return 25;
    }

    inline QString defaultSizeMode(const QString& id)
    {
        for (const Entry& panel : all())
        {
            if (panel.id == id)
                return panel.defaultSizeMode;
        }

        return "fixed";
    }
}
