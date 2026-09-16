#pragma once

#include "Shared.h"

#include "Global.h"
#include "Playout.h"

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

class AbstractRundownWidget;
class QAction;
class QFrame;
class QLabel;
class QMenu;
class QPushButton;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

// A short list of items with their own Play, Stop and Next, for the normal
// layout: the stings, bumpers and one-off graphics an operator wants to hand
// without hunting for them in the rundown.
//
// What it holds are copies. An item dragged in from a rundown or the Library is
// built again here, with that item's settings at the moment it was dropped, and
// from then on it is the Shotbox's own: editing or deleting the rundown item does
// not touch it. There is one Shotbox for the whole client, kept in the database
// (Configuration "ShotboxItems", the same XML a rundown file uses), so it is the
// same whatever rundown is open, and it survives a restart.
//
// The copies are real rundown item widgets, kept hidden, so Play, Stop and Next
// do exactly what they do in a rundown - same commands, same channel lock check,
// same preview redirect, same log line - without a second implementation of
// forty item types. The rules that do not need the application are in
// src/Common/ShotboxRules.h and tested by tools/test-shotbox.
class WIDGETS_EXPORT ShotboxPanelWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ShotboxPanelWidget(QWidget* parent = nullptr);
        ~ShotboxPanelWidget();

    protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct Row
        {
            AbstractRundownWidget* item = nullptr;
            QFrame* frame = nullptr;
            QLabel* badge = nullptr;
            QLabel* label = nullptr;
            bool playing = false;
        };

        QTabWidget* tabWidget = nullptr;
        QWidget* page = nullptr;
        QVBoxLayout* rowsLayout = nullptr;
        QLabel* emptyHint = nullptr;

        // Parent of the hidden item widgets. Never shown.
        QWidget* holder = nullptr;

        QList<Row> rows;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;
        bool collapsed = false;

        // A row being dragged to a new place.
        QPoint pressPosition;
        int pressedRow = -1;
        int pendingMoveTo = -1;

        void setupMenus();
        void load();
        void save() const;
        void rebuildRows();
        void updateRowStyle(int index);

        // Builds a copy from rundown XML (<items><item>...</item></items>) or
        // from a Library drag, and adds what fits. Counts go to the drop notice.
        void addFromRundownXml(const QString& xml, int& offered, int& added, int& refused);
        void addFromLibraryDrag(const QString& data, int& offered, int& added, int& refused);
        bool append(AbstractRundownWidget* widget);

        void fire(int index, Playout::PlayoutType type);
        void removeRow(int index);
        void moveRow(int from, int to);
        void clearAll();
        int rowAt(const QPoint& pagePosition) const;

        static bool previewRequested();

        Q_SLOT void toggleExpandCollapse();
};
