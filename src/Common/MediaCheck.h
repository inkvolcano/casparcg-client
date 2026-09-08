#pragma once

// Deciding whether a rundown item points at media that is not there.
//
// The stock client half-tells you this already: open a broken item and the
// Inspector's target field is blank, because the name it holds matches nothing.
// That only helps if you open the item. A rundown of two hundred rows can carry
// a handful of items pointing at clips that were renamed on the server last week,
// and nothing on screen says so until one of them is played on air.
//
// The judgement this header makes is not "does the file exist" — it is "do we
// know enough to say it does not". Those are different, and getting the
// difference wrong is what makes a warning worth ignoring:
//
//   Two sources can vouch for an item. The Library, which a server fills in when
//   it scans, and the media or template folder on this machine, which needs no
//   server at all. Either one is enough.
//
//   When neither source could have known — no library rows for that device, no
//   path configured — the answer is Unknown, not Missing. A client that has
//   never connected to anything would otherwise open a rundown and flag every
//   single row, which teaches the operator to ignore the warning, which is worse
//   than not having it.
//
// Header-only, like PanelFit.h and LayoutPreset.h, so the decision can be
// compiled and tested without linking the widgets or touching a database.

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace MediaCheck
{
    enum class Verdict
    {
        NotApplicable,  // the item does not point at a file
        Present,        // something vouched for it
        Missing,        // something that would have known says no
        Unknown         // nothing could have known, so nothing is claimed
    };

    // Item types that name a file. Everything else — gateways, mixer items,
    // playout commands, separators — has nothing to be missing.
    inline bool typeUsesMedia(const QString& type)
    {
        return type == "MOVIE" || type == "STILL" || type == "AUDIO"
            || type == "TEMPLATE" || type == "IMAGESCROLLER";
    }

    // Templates live under the template path, everything else under media.
    inline bool typeUsesTemplatePath(const QString& type)
    {
        return type == "TEMPLATE";
    }

    // The file extensions worth trying for a name that carries none. CasparCG
    // media names have no extension, so a name has to be tried against the
    // formats a server would have accepted.
    inline QStringList extensionsFor(const QString& type)
    {
        if (type == "TEMPLATE")
            return QStringList() << ".html" << ".htm" << ".ft" << ".ct" << ".swf";

        if (type == "AUDIO")
            return QStringList() << ".wav" << ".mp3" << ".aac" << ".flac" << ".ogg" << ".wma" << ".m4a";

        if (type == "STILL" || type == "IMAGESCROLLER")
            return QStringList() << ".png" << ".jpg" << ".jpeg" << ".bmp" << ".gif"
                                 << ".tif" << ".tiff" << ".webp" << ".tga";

        return QStringList() << ".mov" << ".mp4" << ".mxf" << ".avi" << ".mkv" << ".wmv"
                             << ".webm" << ".mpg" << ".mpeg" << ".ts" << ".m4v" << ".flv";
    }

    // What the two sources found, and whether either was in a position to know.
    struct Evidence
    {
        bool inLibrary = false;      // the Library has a row for this name
        bool libraryUsable = false;  // the Library holds rows for this device at all
        bool onDisk = false;         // a file was found under the configured path
        bool pathUsable = false;     // a path is configured and exists on this machine
    };

    inline Verdict verdictFor(const QString& type, const QString& name, const Evidence& evidence)
    {
        if (!typeUsesMedia(type))
            return Verdict::NotApplicable;

        // An item nobody has filled in yet is not broken, it is unfinished, and
        // flagging it would put a warning on every freshly dragged-in row.
        if (name.trimmed().isEmpty())
            return Verdict::NotApplicable;

        // Either source vouching is enough. They disagree often and legitimately:
        // a library scanned before a file was added, or a venue whose media lives
        // somewhere this machine cannot see.
        if (evidence.inLibrary || evidence.onDisk)
            return Verdict::Present;

        // Neither found it. Only now does it matter whether either could have.
        if (evidence.libraryUsable || evidence.pathUsable)
            return Verdict::Missing;

        return Verdict::Unknown;
    }

    // What to tell the operator. Says which sources were consulted, because
    // "missing" means different things with and without a server and an operator
    // needs to know which one they are looking at.
    inline QString explain(const QString& type, const QString& name, const Evidence& evidence)
    {
        if (verdictFor(type, name, evidence) != Verdict::Missing)
            return QString();

        QString where;
        if (evidence.libraryUsable && evidence.pathUsable)
            where = "It is not in the server's library, and no file of that name is under the configured path.";
        else if (evidence.libraryUsable)
            where = "It is not in the server's library. No media path is configured, so the disk was not checked.";
        else
            where = "No file of that name is under the configured path. The server's library is empty, so it was not checked.";

        return QString("\"%1\" was not found.\n\n%2").arg(name.trimmed(), where);
    }
}
