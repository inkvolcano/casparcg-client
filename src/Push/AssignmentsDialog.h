#pragma once

#include "PushWindow.h"

#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>

class QNetworkAccessManager;

// Which machine gets which packs, as a grid.
//
// One relay or repository carries every project, and each machine takes a subset.
// Deciding that here rather than on each machine is the difference between moving a
// project and visiting a venue.
//
// Rows are machines, columns are packs, and a tick is an assignment. The row named
// "*" is what a machine gets when it is not listed, which is how a shared pack
// reaches an estate without naming every box in it.
//
// Machines arrive from three places, because no one of them is complete: the ones
// that have checked in with the relay, the ones already named in the file, and any
// typed in by hand for a machine that is not built yet. Packs likewise: what the
// source holds, what the file already mentions, and what is in the dev machine's
// own templates folder.
//
// Nothing is written until Save. Load throws away whatever is on screen and asks
// again, which is the way back from a mess.
class AssignmentsDialog : public QDialog
{
    Q_OBJECT

    public:
        // localPacks is what the dev machine has in its templates folder, so a pack
        // can be assigned before it has ever been uploaded.
        explicit AssignmentsDialog(const PushTarget& target, const QStringList& localPacks,
                                   QWidget* parent = nullptr);

    private:
        void buildUi();

        // ---- reading what is there ----
        void load();
        void loadFromRelay();
        void loadFromGitHub();
        void requestGitHubBlob(const QString& sha);

        // Everything gathered, turned into a grid.
        void buildGrid();

        // ---- writing it back ----
        void save();
        void saveToRelay(const QByteArray& body);
        void saveToGitHub(const QByteArray& body);

        // The grid as the map both ends understand.
        QJsonObject collect() const;

        void say(const QString& line, bool bad = false);
        void setBusy(bool busy);

        Q_SLOT void addMachine();
        Q_SLOT void removeMachine();

        PushTarget target;
        QNetworkAccessManager* network = nullptr;

        QTableWidget* grid = nullptr;
        QLabel* status = nullptr;
        QPushButton* loadButton = nullptr;
        QPushButton* saveButton = nullptr;
        QPushButton* addButton = nullptr;
        QPushButton* removeButton = nullptr;

        // What the source said, before anything was edited.
        QJsonObject assignments;

        QStringList machines;
        QStringList packs;
        QStringList localPacks;

        // Which machines have actually reported in, so the grid can say which rows
        // are real and which are only hoped for.
        QStringList seenMachines;

        // A repository needs the blob it is replacing, or the commit is refused.
        QString gitHubBlobSha;

        bool busy = false;
        bool loaded = false;
};
