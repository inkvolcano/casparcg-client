#pragma once

// Finding the OGraf graphics in a template folder, and deciding what to call
// them in the Library.
//
// The client already handles an OGraf graphic everywhere it matters: a rundown
// item of type TEMPLATE whose name resolves to a manifest previews as a graphic
// and fills the Inspector's typed fields from the manifest's schema. The one
// thing missing was any way to find one. The Library lists what a server scanned,
// and a server does not know what OGraf is.
//
// So this is a local walk of the template folder, and the policy for it lives
// here — separately from the walk itself — because the policy is where the cost
// is. A template folder can be enormous, and the Library refreshes on a timer.
//
// Three limits, all deliberate:
//
//   Depth. A graphic is a folder holding its manifest, so the manifest is one
//   level down, or at the root for a folder that is itself one graphic. Beyond
//   that is somebody's asset tree and walking it buys nothing.
//
//   Count. A folder with thousands of manifests is a mistake somewhere, and
//   filling the Library with them helps nobody. Stopping is better than hanging.
//
//   Off by default. None of this runs unless OGraf support is switched on, so a
//   client that does not use it pays nothing at all.

#include <QtCore/QString>

namespace OgrafLibrary
{
    // How far below the template folder to look. 1 means "the template folder and
    // the folders directly inside it" — which is where a graphic's manifest lives.
    inline int maxDepth() { return 2; }

    // Enough for any real estate of graphics, low enough that a wrong folder
    // cannot make the Library unusable.
    inline int maxGraphics() { return 500; }

    // Directories never worth descending into. These are where a graphic keeps
    // its dependencies, not where another graphic lives.
    inline bool isIgnoredDirectory(const QString& name)
    {
        const QString lower = name.toLower();

        return lower == "node_modules" || lower == ".git" || lower == ".svn"
            || lower == "lib" || lower == "libs" || lower == "assets"
            || lower == "fonts" || lower == "images" || lower == "img"
            || lower.startsWith(".");
    }

    // The name a rundown item carries for this graphic, given the manifest's path
    // relative to the template folder.
    //
    // It has to be the name the resolver will look up again later — the same
    // string that finds this manifest from a template path — or an item dragged
    // out of the Library would not preview.
    //
    // "SEVILLE/l3rd.ograf.json"  -> "SEVILLE/l3rd"
    // "l3rd.ograf.json"          -> "l3rd"
    inline QString itemNameFor(const QString& relativeManifestPath)
    {
        QString path = relativeManifestPath;
        path.replace(QChar(0x5C), QChar('/'));

        while (path.startsWith('/'))
            path.remove(0, 1);

        const QString suffix = ".ograf.json";
        if (path.endsWith(suffix, Qt::CaseInsensitive))
            path.chop(suffix.length());

        return path;
    }

    // What to show in the Library. The manifest's own name is meant for people;
    // the file name is the fallback when a manifest has none.
    inline QString displayNameFor(const QString& manifestName, const QString& relativeManifestPath)
    {
        const QString trimmed = manifestName.trimmed();
        if (!trimmed.isEmpty())
            return trimmed;

        const QString itemName = itemNameFor(relativeManifestPath);
        const int slash = itemName.lastIndexOf('/');

        return slash >= 0 ? itemName.mid(slash + 1) : itemName;
    }
}
