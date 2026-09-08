#pragma once

#include "Shared.h"
#include "ui_StatusPanelWidget.h"

#include "Global.h"

#include "Events/Rundown/BankAssignmentChangedEvent.h"
#include "Events/Rundown/ChannelActivityEvent.h"
#include "Events/Rundown/AutoLoopCountdownEvent.h"
#include "Events/Rundown/PlaybackProgressEvent.h"
#include "Models/CasparMedia.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QTimer>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class CasparDevice;

class WIDGETS_EXPORT StatusPanelWidget : public QWidget, Ui::StatusPanelWidget
{
    Q_OBJECT

    public:
        explicit StatusPanelWidget(QWidget* parent = 0);

        QTabWidget* serverTabWidget() { return tabWidgetServer; }
        QTabWidget* activityTabWidget() { return tabWidgetActivity; }
        QTabWidget* banksTabWidget() { return tabWidgetBanks; }

    private:
        bool serverCollapsed;
        bool activityCollapsed;
        bool banksCollapsed;

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
        QToolButton* activityMenuButton = nullptr;
        QToolButton* banksMenuButton = nullptr;
        QMenu* serverMenu = nullptr;
        QMenu* activityMenu = nullptr;
        QMenu* banksMenu = nullptr;
        QAction* serverExpandCollapseAction = nullptr;
        QAction* activityExpandCollapseAction = nullptr;
        QAction* banksExpandCollapseAction = nullptr;

        // Visibility settings
        bool showPVW;
        bool showServers;
        bool showChannelLocks;
        bool showChannelHeaders;
        bool showBankIcons;
        bool showNoActivity = true;
        bool bigBoldMode = false;
        QString disconnectMode;  // "hidden", "direct", "ask"

        // Activity entries (video with progress, or static items)
        struct ActivityEntry
        {
            QWidget* row = nullptr;
            QLabel* labelTypeBadge = nullptr;
            QLabel* labelInfo = nullptr;
            QLabel* labelLayer = nullptr;
            QLabel* labelIcons = nullptr;
            QLabel* labelTime = nullptr;
            QProgressBar* progressBar = nullptr;
            QPropertyAnimation* animation = nullptr;
            QGraphicsOpacityEffect* rowOpacity = nullptr;
            QPropertyAnimation* rowFadeAnim = nullptr;  // Current opacity fade; stopped before starting a new one.
            qint64 lastUpdate = 0;
            double fps = 0;
            int channel = 0;
            int videolayer = 0;
            bool loop = false;
            bool done = false;
            bool isStatic = false;
            bool suppressed = false;
            QString itemType;
            QString label;
        };

        QVBoxLayout* activityLayout;
        QVBoxLayout* activityOuterLayout = nullptr;
        QLabel* noActivityLabel = nullptr;
        QMap<QString, ActivityEntry> activityEntries;
        QMap<int, QLabel*> channelHeaders;  // Channel number -> header label
        QTimer* cleanupTimer;

        // Bank icons (B1-B9 indicators)
        static const int BANK_COUNT = 9;
        QLabel* bankIcons[BANK_COUNT];
        QWidget* bankIconsContainer;

        // Bank entries (assigned banks)
        struct BankEntry
        {
            QWidget* row;
            QLabel* labelBank;
            QLabel* labelItem;
            QLabel* labelChannel;
        };

        QVBoxLayout* banksEntriesLayout;
        QMap<int, BankEntry> bankEntries;

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
        QMap<QString, QMap<int, QPushButton*>> timerButtons; // deviceName -> (channel -> ⏱ button)
        QMap<int, QPushButton*> globalLockButtons;           // channel -> button
        int maxChannels;
        QPushButton* previewModeButton;
        QPushButton* autostepModeButton;
        bool modifierHeld = false;
        bool showSTEP;

        void setupMenus();
        void setupServerPanel();
        void setupActivityPanel();
        void setupBanksPanel();
        void rebuildChannelLockGrid();
        void updateLockButtonStyle(QPushButton* button, bool locked, bool timed = false, int remainingSecs = 0);
        void updateTimerButtonStyle(QPushButton* button, bool active);
        void reorderActivity();
        void removeActivityEntry(const QString& key);
        void updateNoActivityLabel();
        void updateBankIcons();
        void updateBankDisplay(int bankId);
        void removeBankEntry(int bankId);
        void rebuildAllBankEntries();
        void clearAllActivityEntries();
        QString typeBadgeColor(const QString& itemType) const;
        QString typeBadgeLabel(const QString& itemType) const;
        QString channelColorStyle(int channel) const;

        Q_SLOT void toggleServerCollapse();
        Q_SLOT void toggleActivityCollapse();
        Q_SLOT void toggleBanksCollapse();
        Q_SLOT void channelActivity(const ChannelActivityEvent&);
        Q_SLOT void playbackProgress(const PlaybackProgressEvent&);
        Q_SLOT void autoLoopCountdown(const AutoLoopCountdownEvent&);
        Q_SLOT void channelCleared(const QString& deviceName, int channel, int videolayer);
        Q_SLOT void cleanupStaleEntries();
        Q_SLOT void bankAssignmentChanged(const BankAssignmentChangedEvent&);
        Q_SLOT void deviceAdded(CasparDevice&);
        Q_SLOT void deviceRemoved();
        Q_SLOT void deviceConnectionStateChanged(CasparDevice&);
        Q_SLOT void deviceMediaChanged(const QList<CasparMedia>&, CasparDevice&);
        Q_SLOT void channelLockChanged(const QString& deviceName, int channel, bool locked);
        Q_SLOT void timedLockTick(const QString& deviceName, int channel, int remainingSecs);
        Q_SLOT void previewModeChanged(bool active);
        Q_SLOT void previewModifierHeld(bool held);
        Q_SLOT void autostepModeChanged(bool active);
        Q_SLOT void bigBoldModeChangedSlot(bool active);

        void updatePreviewButtonStyle();
        void updateAutostepButtonStyle();
};
