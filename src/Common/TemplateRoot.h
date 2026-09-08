#pragma once

// What the push tool is pointed at, and whether it is pointed at anything.
//
// This exists because of a specific, quiet failure. The templates folder is a
// text field with a placeholder showing an example path. Leave it empty and the
// old code did `QDir("")` — and Qt resolves an empty path to the program's
// working directory, and reports that it exists. So the tool listed the folders
// sitting next to its own executable and offered them as template packs:
// platforms, imageformats, qmltooling, resources. Qt's own runtime, presented as
// something to push to a playout machine.
//
// Listing them was the visible half. The dangerous half was that every later
// step used the same empty root: `QDir("").filePath("platforms")` is "platforms",
// a relative path, which resolves against the working directory too. Comparing
// walked Qt's DLLs, and pushing would have uploaded them into a client's
// templates folder under a pack called "platforms".
//
// So the root is not a string any more. It is a question with three answers —
// set and usable, set and wrong, or not set — and nothing downstream is allowed
// to proceed without one.
//
// Header-only so the rule can be compiled and tested without linking the tool.

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>

namespace TemplateRoot
{
    struct Root
    {
        bool usable = false;
        QString path;      // absolute, when usable
        QString problem;   // what to tell the operator, when not
    };

    inline Root resolve(const QString& typed)
    {
        Root root;

        const QString trimmed = typed.trimmed();

        if (trimmed.isEmpty())
        {
            root.problem = "No templates folder is set. Use Browse to pick one.";
            return root;
        }

        // A relative path is the same trap wearing a different hat: it resolves
        // against wherever the tool happens to have been started from, which is
        // never what somebody typing a templates folder meant. Browse always
        // gives an absolute one.
        const QFileInfo info(trimmed);
        if (!info.isAbsolute())
        {
            root.problem = QString("\"%1\" is a relative path. Give the full path to the folder.").arg(trimmed);
            return root;
        }

        if (!info.exists())
        {
            root.problem = QString("\"%1\" does not exist.").arg(trimmed);
            return root;
        }

        if (!info.isDir())
        {
            root.problem = QString("\"%1\" is a file, not a folder.").arg(trimmed);
            return root;
        }

        root.usable = true;
        root.path = QDir(trimmed).absolutePath();

        return root;
    }

    // The folder a pack lives in, or empty when it would not be inside the root.
    //
    // Pack names come from listing the root, so they are ordinarily safe. This is
    // here because they also come back from a saved setting, and a name that
    // climbed out of the root would have the tool comparing — and uploading —
    // something nobody chose.
    inline QString packPath(const Root& root, const QString& pack)
    {
        if (!root.usable)
            return QString();

        const QString name = pack.trimmed();

        if (name.isEmpty() || name == "." || name == "..")
            return QString();

        if (name.contains('/') || name.contains(QChar(0x5C)) || name.contains(':'))
            return QString();

        const QString candidate = QDir(root.path).filePath(name);

        // Belt and braces: the answer has to be inside the root even after the
        // filesystem has had its say about the path.
        const QString cleaned = QDir::cleanPath(candidate);
        const QString base = QDir::cleanPath(root.path);

        if (!cleaned.startsWith(base + "/") && cleaned != base)
            return QString();

        return cleaned;
    }
}
