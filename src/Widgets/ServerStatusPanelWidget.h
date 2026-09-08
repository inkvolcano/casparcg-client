#pragma once

#include "Shared.h"
#include "ui_ServerStatusPanelWidget.h"

#include "Global.h"

#include "Models/CasparMedia.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class CasparDevice;

// Which servers are answering, which channels are locked, and whether the things
// that quietly stop working — the hosted sheet cache, the template relay — still
// are.
//
// Was one of three panels inside a single 2090-line widget. They shared a file
// and almost nothing else: not one of Activity's handlers touched this panel's
// state, or the other way round.
class WIDGETS_EXPORT ServerStatusPanelWidget : public QWidget, Ui::ServerStatusPanelWidget
{
    Q_OBJECT

    public:
        explicit ServerStatusPanelWidget(QWidget* parent = 0);

        // The layout places this tab widget, not the panel around it, which is
        // how the panel keeps its own header and menu.
        QTabWidget* tabWidget() { return tabWidgetServer; }

    private:
        bool serverPlaced = true;
        bool serverCollapsed;

        // The hosted sheet cache, shown beside the playout servers because it is one
        // more thing that is either answering or not while a show is on.
        QWidget* cacheRow = nullptr;
        QLabel* cacheDot = nullptr;
        QLabel* cacheLabel = nullptr;
        QPushButton* cacheBypassButton = nullptr;
        QTimer cacheStatusTimer;

        void setupCacheRow(QVBoxLayout* serverOuterLayout);
        Q_SLOT void updateCacheStatus();

        QWidget* relayRow = nullptr;
        QLabel* relayDot = nullptr;
        QLabel* relayLabel = nullptr;
        QPushButton* relayCheckButton = nullptr;
        void setupRelayRow(QVBoxLayout* serverOuterLayout);
        Q_SLOT void updateRelayStatus();

        QToolButton* serverMenuButton = nullptr;
        QMenu* serverMenu = nullptr;
        QAction* serverExpandCollapseAction = nullptr;

        bool showPVW;
        bool showServers;
        bool showChannelLocks;
        bool showSTEP;
        QString disconnectMode;  // "hidden", "direct", "ask"

        // Server entries (per-device connection status, media status, connect/disconnect)
        struct ServerEntry
        {
            QWidget* row;
            QLabel* labelName;
            QLabel* labelConnectionDot;
            QLabel* labelMediaDot;
            QPushButton* buttonConnect;
            QPushButton* buttonStart = nullptr;
            QPushButton* buttonRestart = nullptr;
            QProcess* serverProcess = nullptr;
            QString deviceName;
            QString serverPath;
            bool connected;
            bool mediaReceived;
        };

        QVBoxLayout* serverLayout;
        QMap<QString, ServerEntry> serverEntries;

        // Channel lock grid
        QWidget* channelLockGrid;
        QMap<QString, QMap<int, QPushButton*>> lockButtons;  // deviceName -> (channel -> button)
        QMap<QString, QMap<int, QPushButton*>> timerButtons; // deviceName -> (channel -> button)
        QMap<int, QPushButton*> globalLockButtons;           // channel -> button
        int maxChannels;
        QPushButton* previewModeButton;
        QPushButton* autostepModeButton;
        bool modifierHeld = false;

        void setupMenus();
        void updatePlacement();

        void setupServerPanel();
        void rebuildChannelLockGrid();
        void updateLockButtonStyle(QPushButton* button, bool locked, bool timed = false, int remainingSecs = 0);
        void updateTimerButtonStyle(QPushButton* button, bool active);
        void updatePreviewButtonStyle();
        void updateAutostepButtonStyle();

        Q_SLOT void toggleServerCollapse();
        Q_SLOT void deviceAdded(CasparDevice&);
        Q_SLOT void deviceRemoved();
        Q_SLOT void deviceConnectionStateChanged(CasparDevice&);
        Q_SLOT void deviceMediaChanged(const QList<CasparMedia>&, CasparDevice&);
        Q_SLOT void channelLockChanged(const QString& deviceName, int channel, bool locked);
        Q_SLOT void timedLockTick(const QString& deviceName, int channel, int remainingSecs);
        Q_SLOT void previewModeChanged(bool active);
        Q_SLOT void previewModifierHeld(bool held);
        Q_SLOT void autostepModeChanged(bool active);
};
