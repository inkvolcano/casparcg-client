#pragma once

#include "Shared.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QObject>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE
class QTimer;
class QTreeWidgetItem;
class QWidget;
QT_END_NAMESPACE

class CasparDevice;
class CasparMedia;
class GroupCommand;
class RundownTreeWidget;

// Watch folders (build 317): a group set to watch a folder inside a server's
// media (Inspector > Watch a media folder) gets an item at its end for every
// new file there - a clip, a still or an audio item, on the channel and layer
// set for the watch.
//
// The server's own media list is the source, so a server on another machine
// works the same as one on this machine: every watched server is asked for it
// on an interval (Settings > Rundown > Watch folders check every, 15 s unless
// set), and any listing that arrives - a manual Library refresh too - is looked
// at. An unchanged listing costs the Library nothing (LibraryManager writes
// only what changed).
//
// What the group has had is saved with it, so an item the operator deletes is
// not put back, and an undo that takes the group back before an addition
// gets it again on the next look. Additions are not undo steps and never move
// the selection. A file that leaves the folder leaves its item where it is,
// marked missing. A locked rundown is left alone. The rules are in
// Common/WatchFolderRules.h (tools/test-watchfolder).
class WIDGETS_EXPORT WatchFolders : public QObject
{
    Q_OBJECT

    public:
        static WatchFolders& getInstance();

        // Where the open rundowns are (MainWindow's rundown area).
        void setArea(QWidget* rundownArea);

        static int intervalSeconds();

        // Asks every watched server for its media now.
        void poll();

        // The media names a server's Library holds, for the Inspector.
        static QStringList libraryNames(const QString& server);

    private:
        explicit WatchFolders();

        QPointer<QWidget> area;
        QTimer* timer = nullptr;

        struct Watch
        {
            QPointer<RundownTreeWidget> rundown;
            QTreeWidgetItem* item = nullptr;
            GroupCommand* group = nullptr;
        };
        QList<Watch> watches() const;

        // The items each watch last found gone, so the rundown is swept only
        // when that changes. Keyed by address and only compared, never followed.
        QHash<const GroupCommand*, QStringList> goneBefore;

        void mediaArrived(const QList<CasparMedia>& media, CasparDevice& device);
        void apply(const QList<CasparMedia>& media, const QString& server);
};
