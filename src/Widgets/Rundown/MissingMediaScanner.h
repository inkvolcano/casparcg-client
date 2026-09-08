#pragma once

#include "../Shared.h"

#include "MediaCheck.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
QT_END_NAMESPACE

// Marks rundown rows whose media is not there.
//
// It works from outside the rundown widgets rather than inside them. Every one
// of the forty-odd item widgets already has a "frameItem" to hang a badge on, so
// the marker is created as a child of that from here — which means no item type
// has to know this feature exists, and a type added later gets it for free.
//
// The Library is read once per sweep rather than once per item: a two hundred row
// rundown would otherwise be two hundred queries on every load.
class WIDGETS_EXPORT MissingMediaScanner : public QObject
{
    Q_OBJECT

    public:
        explicit MissingMediaScanner(QObject* parent = nullptr);

        // Whether the sweep runs at all. Off puts every marker away.
        static bool isEnabled();

        struct Result
        {
            int checked = 0;
            int missing = 0;
            int unknown = 0;
        };

        // Walks the tree, marking rows and clearing markers that no longer apply.
        Result scan(QTreeWidget* tree);

    private:
        // name|deviceName for everything the Library knows, and the set of
        // devices it holds anything for. Rebuilt at the start of each sweep.
        QSet<QString> libraryKeys;
        QSet<QString> devicesWithLibrary;

        // Media and template folders per device, resolved once per sweep, with
        // whether each is a folder this machine can actually see.
        struct Paths
        {
            QString mediaPath;
            QString templatePath;
            bool mediaUsable = false;
            bool templateUsable = false;
        };
        QHash<QString, Paths> devicePaths;

        void buildLibraryIndex();
        Paths pathsFor(const QString& deviceName);
        bool existsUnder(const QString& folder, const QString& name, const QString& type) const;

        void scanItem(QTreeWidget* tree, QTreeWidgetItem* item, Result& result);
        void mark(QWidget* widget, const QString& tooltip);
        void clearMark(QWidget* widget);
};
