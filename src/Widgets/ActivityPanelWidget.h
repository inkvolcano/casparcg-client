#pragma once

#include "Shared.h"
#include "ui_ActivityPanelWidget.h"

#include "Global.h"

#include "Events/Rundown/ChannelActivityEvent.h"
#include "Events/Rundown/AutoLoopCountdownEvent.h"
#include "Events/Rundown/PlaybackProgressEvent.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

// What is on each channel and layer right now, with progress.
//
// The busiest panel in the client: one playback-progress event arrives per
// playing layer at the OSC polling rate, five to twenty a second during a show,
// and each rebuilds a row. That is why it asks whether it is placed before doing
// anything — the signal reaches a hidden widget just as readily as a visible one.
class WIDGETS_EXPORT ActivityPanelWidget : public QWidget, Ui::ActivityPanelWidget
{
    Q_OBJECT

    public:
        explicit ActivityPanelWidget(QWidget* parent = 0);

        QTabWidget* tabWidget() { return tabWidgetActivity; }

    private:
        bool activityPlaced = true;
        bool activityCollapsed;

        QToolButton* activityMenuButton = nullptr;
        QMenu* activityMenu = nullptr;
        QAction* activityExpandCollapseAction = nullptr;

        bool showChannelHeaders;
        bool showNoActivity = true;
        bool bigBoldMode = false;

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
        QTimer* cleanupTimer = nullptr;

        void setupMenus();
        void updatePlacement();

        void setupActivityPanel();
        void reorderActivity();
        void removeActivityEntry(const QString& key);
        void updateNoActivityLabel();
        void clearAllActivityEntries();
        QString typeBadgeColor(const QString& itemType) const;
        QString typeBadgeLabel(const QString& itemType) const;
        QString channelColorStyle(int channel) const;

        Q_SLOT void toggleActivityCollapse();
        Q_SLOT void channelActivity(const ChannelActivityEvent&);
        Q_SLOT void playbackProgress(const PlaybackProgressEvent&);
        Q_SLOT void autoLoopCountdown(const AutoLoopCountdownEvent&);
        Q_SLOT void channelCleared(const QString& deviceName, int channel, int videolayer);
        Q_SLOT void cleanupStaleEntries();
        Q_SLOT void bigBoldModeChangedSlot(bool active);
};
