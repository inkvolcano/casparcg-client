#include "ActivityPanelWidget.h"

#include "PanelPlacement.h"
#include "ChannelBadge.h"

#include "Global.h"
#include "PanelHelper.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "RelayClient.h"
#include "SheetCacheServer.h"
#include "TriggerBankRegistry.h"
#include "Timecode.h"
#include "Rundown/AbstractRundownWidget.h"

#include "CasparDevice.h"

#include <algorithm>

#include "DatabaseManager.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTreeWidget>

ActivityPanelWidget::ActivityPanelWidget(QWidget* parent)
    : QWidget(parent),
      activityCollapsed(false)
{
    setupUi(this);

    // Grows to fit its content rather than holding a fixed share of the column.
    this->tabWidgetActivity->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QString val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowChannelHeaders").getValue();
    this->showChannelHeaders = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowNoActivity").getValue();
    this->showNoActivity = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("BigBoldMode").getValue();
    this->bigBoldMode = (val == "true");
    EventManager::getInstance().setBigBoldMode(this->bigBoldMode);

    // The Activity panel auto-grows to fit content, with a floor the operator sets.
    this->widgetActivity->setMinimumHeight(0);
    int actMinH = DatabaseManager::getInstance()
        .getConfigurationByName("MinHeight_Activity").getValue().toInt();
    if (actMinH > 0)
        this->tabWidgetActivity->setMinimumHeight(actMinH);

    setupMenus();
    setupActivityPanel();

    // Sweeps rows whose player has stopped talking. Started by updatePlacement(),
    // and only when this panel is somewhere.
    this->cleanupTimer = new QTimer(this);
    this->cleanupTimer->setInterval(500);
    QObject::connect(this->cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanupStaleEntries()));

    updatePlacement();

    // The layout decides whether any of this runs, so the answer is re-read
    // whenever the layout changes rather than only at startup.
    QObject::connect(&EventManager::getInstance(), &EventManager::rebuildLayout,
                     this, [this]() { updatePlacement(); });

    QObject::connect(&EventManager::getInstance(), SIGNAL(playbackProgress(const PlaybackProgressEvent&)),
                     this, SLOT(playbackProgress(const PlaybackProgressEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(channelActivity(const ChannelActivityEvent&)),
                     this, SLOT(channelActivity(const ChannelActivityEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autoLoopCountdown(const AutoLoopCountdownEvent&)),
                     this, SLOT(autoLoopCountdown(const AutoLoopCountdownEvent&)));

    // Clear CH / Clear VL / Clear Output wipe every row on the affected channel(/layer).
    QObject::connect(&EventManager::getInstance(), &EventManager::channelCleared,
                     this, &ActivityPanelWidget::channelCleared);

    QObject::connect(&EventManager::getInstance(), SIGNAL(bigBoldModeChanged(bool)),
                     this, SLOT(bigBoldModeChangedSlot(bool)));
}

// Whether this panel is anywhere in the layout. A panel that is not placed does
// no work at all — not merely no drawing, because a hidden widget still receives
// the signal and the handler still runs.
void ActivityPanelWidget::updatePlacement()
{
    const bool simpleMode = DatabaseManager::getInstance()
        .getConfigurationByName("SimpleMode").getValue() == "true";

    QStringList columns;
    foreach (const QString& key, PanelPlacement::columnKeys(simpleMode))
        columns.append(DatabaseManager::getInstance().getConfigurationByName(key).getValue());

    this->activityPlaced = PanelPlacement::isPlaced("Activity", columns);

    if (this->cleanupTimer != nullptr)
    {
        if (this->activityPlaced && !this->cleanupTimer->isActive())
            this->cleanupTimer->start();
        else if (!this->activityPlaced && this->cleanupTimer->isActive())
            this->cleanupTimer->stop();
    }
}

void ActivityPanelWidget::setupMenus()
{
    // Activity hamburger menu.
    this->activityMenu = new QMenu(this);
    this->activityMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->activityMenu, "Activity", this);
    this->activityMenu->addSeparator();

    // "Show No Activity" toggle.
    QAction* noActivityAction = this->activityMenu->addAction("Show No Activity");
    noActivityAction->setCheckable(true);
    noActivityAction->setChecked(this->showNoActivity);
    QObject::connect(noActivityAction, &QAction::toggled, this, [this](bool checked) {
        this->showNoActivity = checked;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ShowNoActivity", checked ? "true" : "false"));
        updateNoActivityLabel();
    });

    // "Big & Bold" display mode toggle.
    QAction* bigBoldAction = this->activityMenu->addAction("Big & Bold Mode");
    bigBoldAction->setCheckable(true);
    bigBoldAction->setChecked(this->bigBoldMode);
    QObject::connect(bigBoldAction, &QAction::toggled, this, [this](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "BigBoldMode", checked ? "true" : "false"));
        EventManager::getInstance().setBigBoldMode(checked);
    });

    // "Minimum Height" submenu with presets.
    {
        QMenu* minHeightMenu = new QMenu("Minimum Height", this);
        QActionGroup* minHGroup = new QActionGroup(minHeightMenu);
        minHGroup->setExclusive(true);
        int currentMinH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_Activity").getValue().toInt();
        for (int h : {0, 30, 50, 80, 100})
        {
            QAction* a = minHeightMenu->addAction(h == 0 ? "None" : QString("%1 px").arg(h));
            a->setCheckable(true);
            a->setData(h);
            a->setChecked(h == currentMinH);
            minHGroup->addAction(a);
        }
        QObject::connect(minHGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            int h = action->data().toInt();
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "MinHeight_Activity", QString::number(h)));
            this->tabWidgetActivity->setMinimumHeight(h);
        });
        this->activityMenu->addMenu(minHeightMenu);
    }

    this->activityMenu->addSeparator();
    this->activityExpandCollapseAction = this->activityMenu->addAction("Collapse", this, &ActivityPanelWidget::toggleActivityCollapse);

    this->activityMenuButton = new QToolButton(this->tabWidgetActivity);
    this->activityMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->activityMenuButton->setFixedSize(22, 22);
    this->activityMenuButton->setMenu(this->activityMenu);
    this->activityMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetActivity->setCornerWidget(this->activityMenuButton);
}


void ActivityPanelWidget::setupActivityPanel()
{
    // Set up Activity content layout.
    this->activityOuterLayout = new QVBoxLayout(this->widgetActivity);
    this->activityOuterLayout->setContentsMargins(4, 2, 4, 2);
    this->activityOuterLayout->setSpacing(2);

    this->activityLayout = new QVBoxLayout();
    this->activityLayout->setSpacing(this->bigBoldMode ? 5 : 2);
    this->activityOuterLayout->addLayout(this->activityLayout);
    this->activityOuterLayout->addStretch();  // Push activities to top

    updateNoActivityLabel();
}

void ActivityPanelWidget::toggleActivityCollapse()
{
    this->activityCollapsed = !this->activityCollapsed;
    this->widgetActivity->setVisible(!this->activityCollapsed);
    this->activityExpandCollapseAction->setText(this->activityCollapsed ? "Expand" : "Collapse");

    if (this->activityCollapsed)
    {
        this->tabWidgetActivity->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        int minH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_Activity").getValue().toInt();
        this->tabWidgetActivity->setMinimumHeight(qMax(minH, 0));
        this->tabWidgetActivity->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

QString ActivityPanelWidget::typeBadgeColor(const QString& itemType) const
{
    if (itemType == "STILL")
        return "rgba(255, 165, 0, 200)";   // orange
    if (itemType == "TEMPLATE")
        return "rgba(76, 175, 80, 200)";    // green
    if (itemType == "HTML")
        return "rgba(0, 188, 212, 200)";    // cyan
    if (itemType == "IMAGESCROLLER")
        return "rgba(156, 39, 176, 200)";   // purple
    if (itemType == "AUDIO")
        return "rgba(255, 235, 59, 200)";   // yellow
    if (itemType == "MOVIE")
        return "rgba(33, 150, 243, 200)";   // blue
    if (itemType == "SOLIDCOLOR")
        return "rgba(121, 85, 72, 200)";    // brown
    if (itemType == "DECKLINKINPUT")
        return "rgba(255, 87, 34, 200)";    // deep orange
    if (itemType == "ROUTECHANNEL" || itemType == "ROUTEVIDEOLAYER")
        return "rgba(96, 125, 139, 200)";   // blue-grey

    return "rgba(158, 158, 158, 200)";      // grey default
}

QString ActivityPanelWidget::typeBadgeLabel(const QString& itemType) const
{
    if (itemType == "STILL")
        return "IMAGE";
    if (itemType == "MOVIE")
        return "VIDEO";
    return itemType;
}

QString ActivityPanelWidget::channelColorStyle(int channel) const
{
    return ChannelBadge::style(channel, this->bigBoldMode);
}

void ActivityPanelWidget::removeActivityEntry(const QString& key)
{
    if (!this->activityEntries.contains(key))
        return;

    ActivityEntry& entry = this->activityEntries[key];
    if (entry.rowFadeAnim != nullptr)
    {
        entry.rowFadeAnim->stop();
        delete entry.rowFadeAnim;
        entry.rowFadeAnim = nullptr;
    }
    if (entry.animation != nullptr)
        entry.animation->stop();
    this->activityLayout->removeWidget(entry.row);
    delete entry.row;
    this->activityEntries.remove(key);
    updateNoActivityLabel();
}

void ActivityPanelWidget::updateNoActivityLabel()
{
    bool hasEntries = false;
    for (auto it = this->activityEntries.cbegin(); it != this->activityEntries.cend(); ++it)
    {
        if (!it.value().suppressed)
        {
            hasEntries = true;
            break;
        }
    }

    if (!hasEntries && this->showNoActivity)
    {
        if (!this->noActivityLabel)
        {
            this->noActivityLabel = new QLabel("No Activity", this->widgetActivity);
            this->noActivityLabel->setAlignment(Qt::AlignCenter);
            this->noActivityLabel->setStyleSheet("color: rgba(150, 150, 150, 150); padding: 8px;");
            this->activityOuterLayout->insertWidget(0, this->noActivityLabel);
        }
        this->noActivityLabel->show();
    }
    else if (this->noActivityLabel)
    {
        this->noActivityLabel->hide();
    }
}

void ActivityPanelWidget::channelActivity(const ChannelActivityEvent& event)
{
    if (!this->activityPlaced)
        return;

    QString key = QString("%1:%2").arg(event.getChannel()).arg(event.getVideolayer());

    if (!event.getActive())
    {
        // Item deactivated - remove entry.
        if (this->activityEntries.contains(key))
        {
            removeActivityEntry(key);
            reorderActivity();
        }
        return;
    }

    // Item activated.
    if (this->activityEntries.contains(key))
    {
        ActivityEntry& existing = this->activityEntries[key];
        if (!existing.isStatic && !existing.suppressed)
        {
            // Already tracked as a progress entry via PlaybackProgressEvent.
            // Only update label if the new one is not empty/whitespace.
            QString newLabel = event.getLabel().trimmed();
            if (!newLabel.isEmpty())
            {
                existing.label = newLabel;
                existing.labelInfo->setText(newLabel);
            }
            return;
        }

        // Replace existing entry (static->static, static->video, video->static, or suppressed->new).
        removeActivityEntry(key);
    }

    // Create a new static activity entry.
    ActivityEntry entry;
    entry.isStatic = true;
    entry.itemType = event.getItemType();
    entry.channel = event.getChannel();
    entry.videolayer = event.getVideolayer();
    entry.label = event.getLabel();
    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    entry.row = new QWidget(this->widgetActivity);
    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(4);

    // Type badge
    int sbFS = this->bigBoldMode ? 13 : 9;
    int sbH  = this->bigBoldMode ? 22 : 16;
    entry.labelTypeBadge = new QLabel(entry.row);
    entry.labelTypeBadge->setText(typeBadgeLabel(event.getItemType()));
    entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 3px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
        .arg(typeBadgeColor(event.getItemType())).arg(sbFS));
    entry.labelTypeBadge->setFixedHeight(sbH);
    rowLayout->addWidget(entry.labelTypeBadge, 0);

    // Info label (item name)
    int siFS = this->bigBoldMode ? 14 : 11;
    entry.labelInfo = new QLabel(entry.row);
    entry.labelInfo->setText(event.getLabel());
    entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: white;").arg(siFS));
    entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(entry.labelInfo, 1);

    // Layer label (badge showing channel-layer)
    QSize slSz = this->bigBoldMode ? QSize(48, 30) : QSize(36, 24);
    entry.labelLayer = new QLabel(entry.row);
    entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
    entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
    entry.labelLayer->setAlignment(Qt::AlignCenter);
    entry.labelLayer->setFixedSize(slSz);
    rowLayout->addWidget(entry.labelLayer, 0);

    this->activityLayout->addWidget(entry.row);
    this->activityEntries[key] = entry;

    reorderActivity();
}

void ActivityPanelWidget::channelCleared(const QString& deviceName, int channel, int videolayer)
{
    Q_UNUSED(deviceName); // activity entries are keyed by channel/layer only

    // Remove every row on the cleared channel (or the exact layer when given) —
    // play rows, progress rows and auto-loop countdown rows alike.
    QStringList clearedKeys;
    for (auto it = this->activityEntries.constBegin(); it != this->activityEntries.constEnd(); ++it)
    {
        if (it.value().channel == channel && (videolayer == -1 || it.value().videolayer == videolayer))
            clearedKeys.append(it.key());
    }

    for (const QString& key : clearedKeys)
        removeActivityEntry(key);

    if (!clearedKeys.isEmpty())
        reorderActivity();
}

void ActivityPanelWidget::autoLoopCountdown(const AutoLoopCountdownEvent& event)
{
    if (!this->activityPlaced)
        return;

    // Use a distinct key so this row doesn't collide with the regular play activity row
    // for the same channel/videolayer.
    QString key = QString("autoloop:%1:%2").arg(event.getChannel()).arg(event.getVideolayer());

    if (!event.getActive())
    {
        if (this->activityEntries.contains(key))
        {
            removeActivityEntry(key);
            reorderActivity();
        }
        return;
    }

    int remaining = event.getRemainingSeconds();
    int total = event.getTotalSeconds();
    if (total < 1) total = 1;

    if (this->activityEntries.contains(key))
    {
        ActivityEntry& e = this->activityEntries[key];
        e.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        if (e.progressBar != nullptr)
        {
            e.progressBar->setRange(0, total);
            e.progressBar->setValue(remaining);
            e.progressBar->setFormat(QString("%1s").arg(remaining));
        }
        if (e.labelInfo != nullptr)
            e.labelInfo->setText(event.getLabel());
        return;
    }

    ActivityEntry entry;
    entry.isStatic = false; // Progress-style entry so cleanup can remove stale rows if events stop.
    entry.itemType = event.getItemType();
    entry.channel = event.getChannel();
    entry.videolayer = event.getVideolayer();
    entry.label = event.getLabel();
    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    entry.row = new QWidget(this->widgetActivity);
    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(4);

    int sbFS = this->bigBoldMode ? 13 : 9;
    int sbH  = this->bigBoldMode ? 22 : 16;
    entry.labelTypeBadge = new QLabel(entry.row);
    entry.labelTypeBadge->setText(tr("LOOP"));
    // Distinct color for loop badges — dark cyan/teal.
    entry.labelTypeBadge->setStyleSheet(QString("background-color: #008b8b; color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 1px 4px;").arg(sbFS));
    entry.labelTypeBadge->setFixedHeight(sbH);
    rowLayout->addWidget(entry.labelTypeBadge, 0);

    int siFS = this->bigBoldMode ? 14 : 11;
    entry.labelInfo = new QLabel(entry.row);
    entry.labelInfo->setText(event.getLabel());
    entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: white;").arg(siFS));
    entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(entry.labelInfo, 1);

    entry.progressBar = new QProgressBar(entry.row);
    entry.progressBar->setRange(0, total);
    entry.progressBar->setValue(remaining);
    entry.progressBar->setFormat(QString("%1s").arg(remaining));
    entry.progressBar->setTextVisible(true);
    entry.progressBar->setFixedWidth(this->bigBoldMode ? 90 : 70);
    entry.progressBar->setFixedHeight(sbH);
    rowLayout->addWidget(entry.progressBar, 0);

    QSize slSz = this->bigBoldMode ? QSize(48, 30) : QSize(36, 24);
    entry.labelLayer = new QLabel(entry.row);
    entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
    entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
    entry.labelLayer->setAlignment(Qt::AlignCenter);
    entry.labelLayer->setFixedSize(slSz);
    rowLayout->addWidget(entry.labelLayer, 0);

    this->activityLayout->addWidget(entry.row);
    this->activityEntries[key] = entry;

    reorderActivity();
}

void ActivityPanelWidget::playbackProgress(const PlaybackProgressEvent& event)
{
    // The hottest path in this file: one of these arrives per playing layer at
    // the OSC polling rate. Doing nothing when the panel is not placed is the
    // whole point of the check.
    if (!this->activityPlaced)
        return;

    QString key = QString("%1:%2").arg(event.getChannel()).arg(event.getVideolayer());
    bool isNew = false;
    QString preservedLabel;     // Preserve label from static entry when upgrading
    QString preservedItemType;  // Preserve item type from static entry when upgrading

    if (!this->activityEntries.contains(key))
    {
        isNew = true;
    }
    else if (this->activityEntries[key].isStatic)
    {
        // Upgrade static entry to progress entry when PlaybackProgressEvent arrives.
        // Preserve label from the static entry (which came from executeCommand)
        // since OSC data might not include a label.
        preservedLabel = this->activityEntries[key].label;
        preservedItemType = this->activityEntries[key].itemType;
        removeActivityEntry(key);
        isNew = true;
    }

    if (isNew)
    {
        ActivityEntry entry;
        // Use event's itemType, falling back to preserved type from static entry, then "MOVIE".
        QString eventItemType = event.getItemType();
        if (eventItemType.isEmpty() && !preservedItemType.isEmpty())
            eventItemType = preservedItemType;
        if (eventItemType.isEmpty())
            eventItemType = "MOVIE";
        entry.itemType = eventItemType;
        entry.channel = event.getChannel();
        entry.videolayer = event.getVideolayer();
        // Use preserved label from static entry if OSC label is empty/whitespace.
        // Fall back to channel-layer as last resort.
        QString oscLabel = event.getLabel().trimmed();
        if (!oscLabel.isEmpty())
            entry.label = oscLabel;
        else if (!preservedLabel.isEmpty())
            entry.label = preservedLabel;
        else
            entry.label = QString("%1 %2-%3").arg(typeBadgeLabel(entry.itemType)).arg(event.getChannel()).arg(event.getVideolayer());

        entry.row = new QWidget(this->widgetActivity);

        // Grid layout with overlapping cells: progress bar behind, labels on top.
        QGridLayout* grid = new QGridLayout(entry.row);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);

        // Background layer: progress bar (fills entire row).
        entry.progressBar = new QProgressBar();
        entry.progressBar->setTextVisible(false);
        entry.progressBar->setMaximum(999999999);
        entry.progressBar->setStyleSheet(
            "QProgressBar {"
            "  background-color: rgba(25, 25, 30, 200);"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: rgba(40, 180, 80, 120);"
            "  border-radius: 4px;"
            "}");
        grid->addWidget(entry.progressBar, 0, 0);

        // Foreground layer: single row of labels overlaid on top of progress bar.
        QWidget* overlay = new QWidget();
        overlay->setAttribute(Qt::WA_TranslucentBackground);
        overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        QHBoxLayout* overlayLayout = new QHBoxLayout(overlay);
        int oM = this->bigBoldMode ? 8 : 4;
        overlayLayout->setContentsMargins(6, oM, oM, oM);
        overlayLayout->setSpacing(4);

        // Type badge (far left)
        int pbFS = this->bigBoldMode ? 12 : 8;
        int pbH  = this->bigBoldMode ? 20 : 14;
        entry.labelTypeBadge = new QLabel();
        entry.labelTypeBadge->setText(typeBadgeLabel(entry.itemType));
        entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
            .arg(typeBadgeColor(entry.itemType)).arg(pbFS));
        entry.labelTypeBadge->setFixedHeight(pbH);
        overlayLayout->addWidget(entry.labelTypeBadge, 0);

        // Item name (stretches to fill)
        int piFS = this->bigBoldMode ? 14 : 10;
        entry.labelInfo = new QLabel();
        entry.labelInfo->setText(entry.label);
        entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 170); background: transparent;").arg(piFS));
        entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        overlayLayout->addWidget(entry.labelInfo, 1);

        // Loop/pause icons
        int iiFS = this->bigBoldMode ? 14 : 10;
        entry.labelIcons = new QLabel();
        entry.labelIcons->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 180); background: transparent;").arg(iiFS));
        overlayLayout->addWidget(entry.labelIcons, 0);

        // Countdown time (bold, in focus)
        int tFS = this->bigBoldMode ? 18 : 13;
        entry.labelTime = new QLabel();
        entry.labelTime->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(255, 255, 255, 230); background: transparent;").arg(tFS));
        entry.labelTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        overlayLayout->addWidget(entry.labelTime, 0);

        // Layer badge (far right)
        QSize plSz = this->bigBoldMode ? QSize(42, 26) : QSize(30, 18);
        entry.labelLayer = new QLabel();
        entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
        entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
        entry.labelLayer->setAlignment(Qt::AlignCenter);
        entry.labelLayer->setFixedSize(plSz);
        overlayLayout->addWidget(entry.labelLayer, 0);

        // Add overlay on same cell as progress bar, then raise it to the front.
        grid->addWidget(overlay, 0, 0);
        overlay->raise();

        // Smooth animation for progress bar.
        entry.animation = new QPropertyAnimation(entry.progressBar, "value", entry.progressBar);
        entry.animation->setDuration(200);
        entry.animation->setEasingCurve(QEasingCurve::Linear);

        // Opacity effect for fade animations (grayout and disappear).
        entry.rowOpacity = new QGraphicsOpacityEffect(entry.row);
        entry.rowOpacity->setOpacity(1.0);
        entry.row->setGraphicsEffect(entry.rowOpacity);

        entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

        this->activityLayout->addWidget(entry.row);
        this->activityEntries[key] = entry;
    }

    ActivityEntry& entry = this->activityEntries[key];

    // Skip processing for static entries - they don't have progress bars.
    if (entry.isStatic)
        return;

    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    // If suppressed (auto-hidden after 20s done), silently absorb OSC updates.
    if (entry.suppressed)
    {
        double fps = event.getFps();
        if (fps > 0)
        {
            int rangeMin = static_cast<int>(event.getClip() * fps);
            int rangeMax = static_cast<int>(event.getTotalClip() * fps);
            int targetValue = static_cast<int>(event.getTime() * fps);
            bool isDone = (rangeMax > rangeMin) && (targetValue >= rangeMax - 1);
            if (!isDone)
            {
                // New clip on this layer — remove suppressed entry so the next call recreates it.
                removeActivityEntry(key);
            }
        }
        return;
    }

    entry.fps = event.getFps();

    // Update itemType and badge if the event carries a different type
    // (e.g. a still replacing a movie on the same layer).
    QString eventType = event.getItemType();
    if (!eventType.isEmpty() && eventType != entry.itemType)
    {
        entry.itemType = eventType;
        if (entry.labelTypeBadge != nullptr)
        {
            int ubFS = this->bigBoldMode ? 12 : 8;
            entry.labelTypeBadge->setText(typeBadgeLabel(entry.itemType));
            entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
                .arg(typeBadgeColor(entry.itemType)).arg(ubFS));
        }
    }

    // Use OSC label if available (and not just whitespace), otherwise keep existing label.
    // Don't overwrite a good label with empty OSC data.
    QString newLabel = event.getLabel().trimmed();
    if (!newLabel.isEmpty())
        entry.label = newLabel;
    // If OSC label is empty, keep the existing entry.label (don't clear it).
    bool wasLoop = entry.loop;
    entry.loop = event.getLoop();

    // Update info label (use entry.label which may have been preserved from static entry)
    entry.labelInfo->setText(entry.label);

    // Update icons
    QString icons;
    if (event.getLoop())
        icons += QString::fromUtf8("\xe2\x9f\xb3");  // loop arrow
    if (event.getPaused())
        icons += QString::fromUtf8(" \xe2\x8f\xb8");  // pause symbol
    entry.labelIcons->setText(icons);

    // Update progress bar range and value
    if (entry.fps > 0)
    {
        double inTime = event.getClip();
        double outTime = event.getTotalClip();
        int rangeMin = static_cast<int>(inTime * entry.fps);
        int rangeMax = static_cast<int>(outTime * entry.fps);
        int targetValue = static_cast<int>(event.getTime() * entry.fps);

        entry.progressBar->setRange(rangeMin, rangeMax);

        // Animate smoothly to new value (skip if unchanged, e.g. paused clips).
        if (targetValue != entry.animation->endValue().toInt())
        {
            entry.animation->stop();
            entry.animation->setStartValue(entry.progressBar->value());
            entry.animation->setEndValue(targetValue);
            entry.animation->start();
        }

        // Update time display (countdown - remaining time)
        double remaining = event.getTotalClip() - event.getTime();
        if (remaining < 0) remaining = 0;
        entry.labelTime->setText(Timecode::fromTime(remaining, entry.fps, false));

        // Check if done playing (progress reached end)
        bool isDone = (rangeMax > rangeMin) && (targetValue >= rangeMax - 1);
        if (isDone != entry.done)
        {
            entry.done = isDone;
            int stBFS = this->bigBoldMode ? 12 : 8;
            int stIFS = this->bigBoldMode ? 14 : 10;
            int stTFS = this->bigBoldMode ? 18 : 13;
            int stLFS = this->bigBoldMode ? 14 : 10;
            if (isDone)
            {
                // Stop any existing fade animation before starting a new one.
                if (entry.rowFadeAnim != nullptr)
                {
                    entry.rowFadeAnim->stop();
                    delete entry.rowFadeAnim;
                    entry.rowFadeAnim = nullptr;
                }

                // Gradually fade to gray over 20s, then fade away over 5s.
                // Opacity reduction on the dark background naturally desaturates colors.
                entry.rowFadeAnim = new QPropertyAnimation(entry.rowOpacity, "opacity", entry.row);
                entry.rowFadeAnim->setDuration(25000);
                entry.rowFadeAnim->setKeyValueAt(0.0, entry.rowOpacity->opacity());
                entry.rowFadeAnim->setKeyValueAt(0.8, 0.3);  // Fade to near-gray over 20s
                entry.rowFadeAnim->setKeyValueAt(1.0, 0.0);   // Fade away over 5s
                entry.rowFadeAnim->start();

                QObject::connect(entry.rowFadeAnim, &QPropertyAnimation::finished, this, [this, key]() {
                    if (this->activityEntries.contains(key) && this->activityEntries[key].done
                        && !this->activityEntries[key].suppressed)
                    {
                        this->activityEntries[key].suppressed = true;
                        this->activityEntries[key].row->hide();
                        this->activityEntries[key].rowFadeAnim = nullptr;
                        reorderActivity();
                    }
                });
            }
            else
            {
                entry.progressBar->setStyleSheet(
                    "QProgressBar { background-color: rgba(25, 25, 30, 200); border: none; border-radius: 4px; }"
                    "QProgressBar::chunk { background-color: rgba(33, 120, 200, 60); border-radius: 4px; }");
                entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
                    .arg(typeBadgeColor(entry.itemType)).arg(stBFS));
                entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 170); background: transparent;").arg(stIFS));
                entry.labelTime->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(255, 255, 255, 230); background: transparent;").arg(stTFS));
                entry.labelLayer->setStyleSheet(channelColorStyle(entry.channel));

                // Fade opacity back to 1.0 (loop restart).
                if (entry.rowOpacity != nullptr)
                {
                    // Stop any existing fade animation before starting a new one.
                    if (entry.rowFadeAnim != nullptr)
                    {
                        entry.rowFadeAnim->stop();
                        delete entry.rowFadeAnim;
                        entry.rowFadeAnim = nullptr;
                    }

                    entry.rowFadeAnim = new QPropertyAnimation(entry.rowOpacity, "opacity", entry.row);
                    entry.rowFadeAnim->setDuration(300);
                    entry.rowFadeAnim->setStartValue(entry.rowOpacity->opacity());
                    entry.rowFadeAnim->setEndValue(1.0);
                    entry.rowFadeAnim->start();

                    QObject::connect(entry.rowFadeAnim, &QPropertyAnimation::finished, this, [this, key]() {
                        if (this->activityEntries.contains(key))
                            this->activityEntries[key].rowFadeAnim = nullptr;
                    });
                }
            }
        }
    }

    // Reorder if loop state changed or new entry
    if (isNew || wasLoop != entry.loop)
        reorderActivity();
}

void ActivityPanelWidget::reorderActivity()
{
    // Remove all widgets from layout (without deleting)
    while (this->activityLayout->count() > 0)
        this->activityLayout->takeAt(0);

    // Remove old channel headers
    for (auto it = this->channelHeaders.begin(); it != this->channelHeaders.end(); ++it)
        delete it.value();
    this->channelHeaders.clear();

    // Helper struct to store entry with its sort key
    struct SortableEntry
    {
        QString key;
        QWidget* row;
        int channel;
        int videolayer;
        bool done;
    };

    QList<SortableEntry> activeEntries;
    QList<SortableEntry> doneEntries;

    for (auto it = this->activityEntries.begin(); it != this->activityEntries.end(); ++it)
    {
        if (it.value().suppressed)
            continue; // Suppressed entries are hidden and excluded from layout.

        SortableEntry se;
        se.key = it.key();
        se.row = it.value().row;
        se.done = it.value().done;
        se.channel = it.value().channel;
        se.videolayer = it.value().videolayer;

        if (se.done)
            doneEntries.append(se);
        else
            activeEntries.append(se);
    }

    // Sort active entries: by channel ascending, then by videolayer descending (highest at top)
    std::sort(activeEntries.begin(), activeEntries.end(), [](const SortableEntry& a, const SortableEntry& b) {
        if (a.channel != b.channel)
            return a.channel < b.channel;  // Channel ascending
        return a.videolayer > b.videolayer;  // Videolayer descending (highest first)
    });

    // Sort done entries the same way
    std::sort(doneEntries.begin(), doneEntries.end(), [](const SortableEntry& a, const SortableEntry& b) {
        if (a.channel != b.channel)
            return a.channel < b.channel;
        return a.videolayer > b.videolayer;
    });

    // Collect all unique channels from both active and done entries
    QSet<int> allChannels;
    for (const SortableEntry& se : activeEntries)
        allChannels.insert(se.channel);
    for (const SortableEntry& se : doneEntries)
        allChannels.insert(se.channel);

    // Sort channels
    QList<int> sortedChannels = allChannels.values();
    std::sort(sortedChannels.begin(), sortedChannels.end());

    // Add entries grouped by channel, with active entries first, then done entries
    for (int channel : sortedChannels)
    {
        // Create channel header
        bool hasActive = false;
        for (const SortableEntry& se : activeEntries)
        {
            if (se.channel == channel)
            {
                hasActive = true;
                break;
            }
        }

        int chFS = this->bigBoldMode ? 13 : 10;
        QLabel* header = new QLabel(this->widgetActivity);
        header->setText(QString("Channel %1").arg(channel));
        // Use brighter color if there are active entries, dimmer if only done entries
        if (hasActive)
            header->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(150, 150, 150, 200); padding: 4px 0 2px 0;").arg(chFS));
        else
            header->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(100, 100, 100, 200); padding: 4px 0 2px 0;").arg(chFS));
        header->setVisible(this->showChannelHeaders);
        this->activityLayout->addWidget(header);
        this->channelHeaders[channel] = header;

        // Add active entries for this channel
        for (const SortableEntry& se : activeEntries)
        {
            if (se.channel == channel)
                this->activityLayout->addWidget(se.row);
        }

        // Add done entries for this channel (greyed out, after active)
        for (const SortableEntry& se : doneEntries)
        {
            if (se.channel == channel)
                this->activityLayout->addWidget(se.row);
        }
    }
}

void ActivityPanelWidget::cleanupStaleEntries()
{
    if (!this->activityPlaced)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList toRemove;

    for (auto it = this->activityEntries.begin(); it != this->activityEntries.end(); ++it)
    {
        // Only auto-remove non-static entries (video entries with stale OSC data).
        // Static entries persist until explicitly removed via channelActivity.
        if (!it.value().isStatic && (now - it.value().lastUpdate) > 3000)
            toRemove.append(it.key());
    }

    for (const QString& key : toRemove)
        removeActivityEntry(key);
}

void ActivityPanelWidget::clearAllActivityEntries()
{
    QStringList keys = this->activityEntries.keys();
    for (const QString& key : keys)
        removeActivityEntry(key);
}

void ActivityPanelWidget::bigBoldModeChangedSlot(bool active)
{
    this->bigBoldMode = active;
    this->activityLayout->setSpacing(active ? 5 : 2);
    clearAllActivityEntries();

    // The Trigger Banks panel listens to the same signal and rebuilds its own
    // rows; it is not this panel's business any more.
}
