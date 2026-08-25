#pragma once

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "OscSubscription.h"
#include "OscDeviceManager.h"
#include "CloneGroupRegistry.h"
#include "TriggerBankRegistry.h"
#include "DeviceManager.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "AbstractRundownWidget.h"

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPalette>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

namespace RundownWidgetHelper
{
    // Cached configuration values — read from DB once, reused by all widget constructors.
    // Call invalidateConfigCache() when settings change (e.g. from SettingsDialog).
    struct ConfigCache
    {
        QString delayType;
        bool markUsedItems = false;
        bool useFreezeOnLoad = false;
        bool reverseOscTime = false;
        bool showThumbnailTooltip = false;
        bool loaded = false;

        void load()
        {
            QMap<QString, QString> cfg = DatabaseManager::getInstance().getAllConfigurations();
            delayType = cfg.value("DelayType", "Milliseconds");
            markUsedItems = cfg.value("MarkUsedItems") == "true";
            useFreezeOnLoad = cfg.value("UseFreezeOnLoad") == "true";
            reverseOscTime = cfg.value("ReverseOscTime") == "true";
            showThumbnailTooltip = cfg.value("ShowThumbnailTooltip") == "true";
            loaded = true;
        }
    };

    inline ConfigCache& configCache()
    {
        static ConfigCache cache;
        if (!cache.loaded)
            cache.load();
        return cache;
    }

    inline void invalidateConfigCache()
    {
        configCache().loaded = false;
    }

    inline const QString& cachedDelayType() { return configCache().delayType; }
    inline bool cachedMarkUsedItems() { return configCache().markUsedItems; }
    inline bool cachedUseFreezeOnLoad() { return configCache().useFreezeOnLoad; }
    inline bool cachedReverseOscTime() { return configCache().reverseOscTime; }
    inline bool cachedShowThumbnailTooltip() { return configCache().showThumbnailTooltip; }

    // Cached pixmaps — loaded from resources once, shared across all widgets.
    inline const QPixmap& gpiConnectedPixmap()
    {
        static QPixmap pm(":/Graphics/Images/GpiConnected.png");
        return pm;
    }

    inline const QPixmap& gpiDisconnectedPixmap()
    {
        static QPixmap pm(":/Graphics/Images/GpiDisconnected.png");
        return pm;
    }

    // Log a playout action to the log panel (for OSC/bank triggers that bypass RundownTreeWidget).
    inline void logPlayoutAction(AbstractRundownWidget* widget, Playout::PlayoutType type)
    {
        if (widget == nullptr || widget->getCommand() == nullptr)
            return;

        static const QMap<Playout::PlayoutType, QString> actionNames = {
            {Playout::PlayoutType::Stop, "Stop"}, {Playout::PlayoutType::Play, "Play"},
            {Playout::PlayoutType::PlayNow, "Play"}, {Playout::PlayoutType::Load, "Load"},
            {Playout::PlayoutType::PauseResume, "Pause"}, {Playout::PlayoutType::Next, "Next"},
            {Playout::PlayoutType::Update, "Update"}, {Playout::PlayoutType::Invoke, "Invoke"},
            {Playout::PlayoutType::Preview, "Preview"}, {Playout::PlayoutType::Clear, "Clear"},
            {Playout::PlayoutType::ClearVideoLayer, "Clear VL"}, {Playout::PlayoutType::ClearChannel, "Clear CH"}
        };

        QString action = actionNames.value(type, "Exec");
        QString label = widget->getLibraryModel()->getLabel();
        if (label.isEmpty()) label = widget->getLibraryModel()->getName();
        QString device = widget->getLibraryModel()->getDeviceName();
        int ch = widget->getCommand()->getChannel();
        int vl = widget->getCommand()->getVideolayer();

        emit EventManager::getInstance().playoutAction(action + " (OSC)", label, device, ch, vl);
    }

    // Check if the item's channel is locked. Returns true if locked (should not execute).
    inline bool isItemChannelLocked(AbstractRundownWidget* widget)
    {
        if (widget == nullptr || widget->getCommand() == nullptr)
            return false;
        QString deviceName = widget->getLibraryModel()->getDeviceName();
        int channel = widget->getCommand()->getChannel();
        return DeviceManager::getInstance().isChannelLocked(deviceName, channel);
    }

    static const int BADGE_WIDTH = 55;
    static const int OLD_COLOR_WIDTH = 18;
    static const int BADGE_SHIFT = BADGE_WIDTH - OLD_COLOR_WIDTH; // 18

    inline QColor channelColor(int channel)
    {
        return QColor::fromHslF(ChannelColor::hue(channel) / 360.0, ChannelColor::saturation(), ChannelColor::lightness());
    }

    // Helper class to resize the bottom row container when frameItem resizes.
    class BottomRowResizer : public QObject
    {
    public:
        BottomRowResizer(QWidget* container, int startX, QWidget* parent)
            : QObject(parent), m_container(container), m_startX(startX)
        {
            parent->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (event->type() == QEvent::Resize)
            {
                QWidget* frame = static_cast<QWidget*>(obj);
                m_container->setFixedWidth(frame->width() - m_startX);
            }
            return false;
        }

    private:
        QWidget* m_container;
        int m_startX;
    };

    // Helper class to keep frameStatus anchored to the right edge as an overlay.
    class FrameStatusOverlay : public QObject
    {
    public:
        FrameStatusOverlay(QWidget* statusFrame, QWidget* parent)
            : QObject(parent), m_statusFrame(statusFrame), m_repositioning(false)
        {
            parent->installEventFilter(this);
            statusFrame->installEventFilter(this);
            QTimer::singleShot(0, this, [this]() { reposition(); });
        }

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (m_repositioning) return false;
            if ((obj == parent() && event->type() == QEvent::Resize) ||
                (obj == m_statusFrame && event->type() == QEvent::LayoutRequest))
                reposition();
            return false;
        }

    private:
        void reposition()
        {
            m_repositioning = true;
            QWidget* parentWidget = static_cast<QWidget*>(parent());
            int statusWidth = m_statusFrame->sizeHint().width();
            m_statusFrame->setGeometry(
                parentWidget->width() - statusWidth, 0,
                statusWidth, parentWidget->height());
            m_repositioning = false;
        }

        QWidget* m_statusFrame;
        bool m_repositioning;
    };

    inline void setupBottomRow(QFrame* frameItem, QLabel* labelColor)
    {
        // Collect bottom row labels: y >= 18, height exactly 16.
        QList<QPair<int, QLabel*>> bottomLabels;
        for (QObject* child : frameItem->children())
        {
            QLabel* label = qobject_cast<QLabel*>(child);
            if (label == nullptr || label == labelColor) continue;
            if (label->y() < 18 || label->height() != 16) continue;
            bottomLabels.append(qMakePair(label->x(), label));
        }

        if (bottomLabels.isEmpty()) return;

        std::sort(bottomLabels.begin(), bottomLabels.end(),
                  [](const QPair<int, QLabel*>& a, const QPair<int, QLabel*>& b) {
                      return a.first < b.first;
                  });

        // Use a consistent start position so columns align across all widget types.
        // After badge shift, labelDevice lands at x = 62 + BADGE_SHIFT = 99.
        // Widgets without labelDevice must start at the same x for alignment.
        static const int BOTTOM_ROW_X = 62 + BADGE_SHIFT;
        int startX = BOTTOM_ROW_X;

        // Create container widget for the bottom row.
        QWidget* container = new QWidget(frameItem);
        container->setObjectName("bottomRowContainer");
        container->setAttribute(Qt::WA_TransparentForMouseEvents);
        container->setStyleSheet("background: transparent;");
        container->move(startX, 19);
        container->setFixedHeight(16);
        container->setFixedWidth(frameItem->width() - startX);

        QHBoxLayout* layout = new QHBoxLayout(container);
        layout->setSpacing(8);
        layout->setContentsMargins(0, 0, 0, 0);

        // Fixed-column layout: each column has a canonical meaning so values
        // align vertically across different widget types.
        // Col 0=device, 1=videolayer, 2=delay, 3=duration, 4=remoteTrigger
        static const int NUM_COLUMNS = 5;
        QLabel* columns[NUM_COLUMNS] = {};
        QList<QLabel*> extraLabels;

        for (const auto& pair : bottomLabels)
        {
            QLabel* label = pair.second;
            QString name = label->objectName();

            if (name == "labelDevice")              columns[0] = label;
            else if (name == "labelVideolayer")      columns[1] = label;
            else if (name == "labelDelay")           columns[2] = label;
            else if (name == "labelDuration")        columns[3] = label;
            else if (name == "labelRemoteTriggerId") columns[4] = label;
            else extraLabels.append(label);
        }

        // Place widget-specific labels (GPO port, etc.) in first empty data slot.
        int extraIdx = 0;
        for (int i = 0; i < NUM_COLUMNS && extraIdx < extraLabels.count(); i++)
        {
            if (columns[i] == nullptr)
                columns[i] = extraLabels[extraIdx++];
        }

        // Use fixed columns with spacers so delay/duration/UID align vertically
        // across all widget types, even those without device/videolayer.
        bool useFixedColumns = (bottomLabels.count() >= 2);

        for (int i = 0; i < NUM_COLUMNS; i++)
        {
            if (columns[i] != nullptr)
            {
                QLabel* label = columns[i];
                label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                label->setFixedHeight(16);
                label->setMinimumWidth(0);
                label->setMaximumWidth(16777215);
                layout->addWidget(label, 1);
            }
            else if (useFixedColumns)
            {
                QLabel* spacer = new QLabel(container);
                spacer->setFixedHeight(16);
                spacer->setMinimumWidth(0);
                layout->addWidget(spacer, 1);
            }
        }

        // Delay/duration minimum width.
        if (columns[2] != nullptr && columns[3] != nullptr)
        {
            columns[2]->setMinimumWidth(55);
            columns[3]->setMinimumWidth(55);
        }

        container->show();

        // Keep container width synced with frameItem on resize.
        new BottomRowResizer(container, startX, frameItem);
    }

    inline QString badgeText(int channel, int videolayer)
    {
        if (videolayer > 0)
            return QString("%1-%2").arg(channel).arg(videolayer);
        return QString::number(channel);
    }

    inline void setupChannelBadge(QFrame* frameItem, QLabel* labelColor, int channel, int videolayer = 0)
    {
        labelColor->setFixedSize(BADGE_WIDTH, Rundown::DEFAULT_ITEM_HEIGHT);
        labelColor->setAlignment(Qt::AlignCenter);
        labelColor->setText(badgeText(channel, videolayer));

        QFont font = labelColor->font();
        font.setPixelSize(12);
        font.setWeight(QFont::Black);
        labelColor->setFont(font);

        // Set channel-based background color with dark border for visual separation.
        QColor color = channelColor(channel);
        labelColor->setStyleSheet(QString("background-color: %1; color: white; border: 2px solid #1a1a1a;").arg(color.name()));

        // Shift all sibling widgets in frameItem that are at x >= OLD_COLOR_WIDTH.
        for (QObject* child : frameItem->children())
        {
            QWidget* w = qobject_cast<QWidget*>(child);
            if (w && w != labelColor && w->x() >= OLD_COLOR_WIDTH)
                w->move(w->x() + BADGE_SHIFT, w->y());
        }

        // Convert frameStatus to an overlay so it doesn't steal width from frameItem.
        // Icons float on top of the content rows with transparent background.
        QFrame* frameStatus = frameItem->parentWidget()->findChild<QFrame*>("frameStatus");
        if (frameStatus != nullptr && frameStatus->layout() == nullptr)
        {
            QWidget* topWidget = frameItem->parentWidget();

            // Remove frameStatus from the QHBoxLayout so frameItem takes full width.
            if (topWidget->layout())
                topWidget->layout()->removeWidget(frameStatus);

            // Transparent background — frameItem's background shows through.
            frameStatus->setStyleSheet("background: transparent;");
            frameStatus->raise();

            // Dynamic sizing based on visible children.
            frameStatus->setMinimumWidth(0);
            frameStatus->setMaximumWidth(16777215);
            frameStatus->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

            // Collect children sorted by original x position to preserve order.
            QList<QPair<int, QWidget*>> children;
            for (QObject* child : frameStatus->children())
            {
                QWidget* w = qobject_cast<QWidget*>(child);
                if (w != nullptr)
                    children.append(qMakePair(w->x(), w));
            }
            std::sort(children.begin(), children.end(),
                      [](const QPair<int, QWidget*>& a, const QPair<int, QWidget*>& b) {
                          return a.first < b.first;
                      });

            QHBoxLayout* layout = new QHBoxLayout(frameStatus);
            layout->setSpacing(0);
            layout->setContentsMargins(0, 0, 0, 0);
            for (const auto& pair : children)
                layout->addWidget(pair.second);

            // Keep frameStatus anchored to the right edge as an overlay.
            new FrameStatusOverlay(frameStatus, topWidget);
        }

        // Convert bottom row labels from absolute positioning to an even layout.
        setupBottomRow(frameItem, labelColor);
    }

    inline void updateChannelBadge(QLabel* labelColor, int channel, int videolayer = 0)
    {
        labelColor->setText(badgeText(channel, videolayer));
        QColor color = channelColor(channel);
        labelColor->setStyleSheet(QString("background-color: %1; color: white; border: 2px solid #1a1a1a;").arg(color.name()));
    }

    // Gateway badge: shows symbols instead of channel number.
    // Call after setupChannelBadge() to override text and color.
    inline void setupGatewayBadge(QLabel* labelColor, bool isExit, const QString& badgeColor, const QString& gatewayType = "")
    {
        QString text;
        if (isExit)
            text = QString("0") + QString(QChar(0x279C));  // 0➜
        else
            text = QString(QChar(0x279C)) + QString("0");  // ➜0
        labelColor->setText(text);
        labelColor->setStyleSheet(QString("background-color: %1; color: white; border: 2px solid #1a1a1a;").arg(badgeColor));
    }

    inline QColor activeColor(int channel)
    {
        // Use custom active indicator color if configured, otherwise derive from channel.
        const QString& val = ColorCache::activeIndicator();
        QColor custom(val);
        if (custom.isValid() && !val.isEmpty() && custom.alpha() > 0)
            return custom;
        return QColor::fromHslF(ChannelColor::hue(channel) / 360.0, ChannelColor::activeSaturation(), ChannelColor::activeLightness());
    }

    // Apply or remove the "disabled" visual: italic + gray text on the main label,
    // and a half-opacity dim on the whole row.  Marker property avoids stomping
    // unrelated graphics effects (e.g., the 0.25-opacity from setUsed).
    inline void applyDisabledStyle(QWidget* rowWidget, QLabel* labelLabel, bool disabled)
    {
        if (labelLabel != nullptr)
        {
            QFont font = labelLabel->font();
            font.setItalic(disabled);
            labelLabel->setFont(font);
            labelLabel->setStyleSheet(disabled
                ? "color: rgba(140, 140, 140, 200); font-style: italic;"
                : "");
        }

        if (rowWidget == nullptr)
            return;

        if (disabled)
        {
            QGraphicsOpacityEffect* existing = qobject_cast<QGraphicsOpacityEffect*>(rowWidget->graphicsEffect());
            if (existing == nullptr || existing->property("disabledEffect").toBool() == false)
            {
                QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(rowWidget);
                effect->setOpacity(0.55);
                effect->setProperty("disabledEffect", true);
                rowWidget->setGraphicsEffect(effect);
            }
        }
        else
        {
            QGraphicsOpacityEffect* existing = qobject_cast<QGraphicsOpacityEffect*>(rowWidget->graphicsEffect());
            if (existing != nullptr && existing->property("disabledEffect").toBool() == true)
                rowWidget->setGraphicsEffect(nullptr);
        }
    }

    inline void setActiveColorPalette(QWidget* label, int channel)
    {
        QColor c = activeColor(channel);
        label->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    }

    inline void clearActiveColorPalette(QWidget* label)
    {
        label->setStyleSheet(QString());
    }

    inline void setSelectionHighlight(QWidget* widget, bool highlight)
    {
        QLabel* activeColor = widget->findChild<QLabel*>("labelActiveColor");
        QLabel* labelColor = widget->findChild<QLabel*>("labelColor");

        if (highlight)
        {
            if (activeColor)
                activeColor->setStyleSheet("background-color: rgba(0, 255, 0, 255);");
            if (labelColor)
            {
                QString style = labelColor->styleSheet();
                if (!style.contains("border-left-color"))
                    labelColor->setStyleSheet(style + " border-left-color: rgba(0, 255, 0, 255);");
            }
        }
        else
        {
            if (activeColor)
                clearActiveColorPalette(activeColor);
            if (labelColor)
            {
                QString style = labelColor->styleSheet();
                style.remove(" border-left-color: rgba(0, 255, 0, 255);");
                labelColor->setStyleSheet(style);
            }
        }
    }

    inline QLabel* createBankBadge(QWidget* parent)
    {
        QLabel* badge = new QLabel(parent);
        badge->setObjectName("labelBankBadge");
        badge->setFixedSize(24, 14);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet("background-color: rgba(255, 165, 0, 200); color: white; border-radius: 3px; font-size: 10px; font-weight: bold;");
        badge->move(BADGE_WIDTH, 2);
        badge->hide();
        return badge;
    }

    inline void updateBankBadge(QLabel* badge, int bankId)
    {
        if (bankId > 0)
        {
            badge->setText(QString("B%1").arg(bankId));
            badge->show();
        }
        else
        {
            badge->hide();
        }
    }

    inline QLabel* createCloneBadge(QWidget* parent)
    {
        QLabel* badge = new QLabel(parent);
        badge->setObjectName("labelCloneBadge");
        badge->setFixedSize(14, 14);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet("background-color: rgba(100, 180, 255, 200); color: white; border-radius: 3px; font-size: 10px; font-weight: bold;");
        badge->move(BADGE_WIDTH, 18);
        badge->setText(QString::fromUtf8("\xf0\x9f\x94\x97")); // 🔗 link emoji
        badge->hide();
        return badge;
    }

    inline void updateCloneBadge(QLabel* badge, const QString& cloneGroupId)
    {
        if (!cloneGroupId.isEmpty())
            badge->show();
        else
            badge->hide();
    }

    inline void setupCloneSupport(QWidget* widget, QFrame* frameItem, AbstractCommand* command)
    {
        QLabel* cloneBadge = createCloneBadge(frameItem);
        QObject::connect(command, &AbstractCommand::cloneGroupIdChanged, [cloneBadge](const QString& id) {
            updateCloneBadge(cloneBadge, id);
        });
        updateCloneBadge(cloneBadge, command->getCloneGroupId());

        QObject::connect(command, &AbstractCommand::propertyChanged, widget, [command]() {
            if (!command->getCloneGroupId().isEmpty())
                CloneGroupRegistry::getInstance().syncFromSource(command->getCloneGroupId(), command);
        });
    }

    inline QString getDelayUnit()
    {
        return DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();
    }

    inline QString getDurationUnit()
    {
        return DatabaseManager::getInstance().getConfigurationByName("DurationUnit").getValue();
    }

    inline QString getDurationFormat()
    {
        return DatabaseManager::getInstance().getConfigurationByName("DurationFormat").getValue();
    }

    inline QString formatTimecode(double seconds)
    {
        if (seconds <= 0)
            return QString();

        int totalSeconds = static_cast<int>(seconds);
        int h = totalSeconds / 3600;
        int m = (totalSeconds % 3600) / 60;
        int s = totalSeconds % 60;
        return QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }

    inline QString humanReadableDuration(double seconds)
    {
        if (seconds <= 0)
            return QString();

        if (seconds < 1.0)
            return QString("%1s").arg(seconds, 0, 'f', 1);

        int totalSeconds = static_cast<int>(seconds);
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int secs = totalSeconds % 60;

        if (hours > 0)
            return QString("%1h%2m").arg(hours).arg(minutes);
        if (minutes > 0)
            return QString("%1m%2s").arg(minutes).arg(secs);
        return QString("%1s").arg(secs);
    }

    inline double getChannelFps(const QString& deviceName, int channel)
    {
        if (deviceName.isEmpty() || channel <= 0)
            return 0;

        const QStringList channelFormats = DatabaseManager::getInstance().getDeviceByName(deviceName).getChannelFormats().split(",");
        if (channel > channelFormats.count())
            return 0;

        return DatabaseManager::getInstance().getFormat(channelFormats[channel - 1]).getFramesPerSecond().toDouble();
    }

    inline QString formatSeconds(double seconds)
    {
        if (seconds <= 0)
            return QString();

        QString format = getDurationFormat();
        if (format == "Timecode")
            return formatTimecode(seconds);
        return humanReadableDuration(seconds);
    }

    inline QString formatDelay(int value, const QString& /*delayType*/, double fps = 0)
    {
        if (value <= 0)
            return QString::fromUtf8("\xe2\x8f\xb3 0");

        QString unit = getDelayUnit();
        double seconds = 0;
        if (unit == Output::DEFAULT_DELAY_IN_MILLISECONDS)
            seconds = value / 1000.0;
        else
        {
            if (fps <= 0) fps = 50.0;
            seconds = value / fps;
        }

        QString readable = formatSeconds(seconds);
        return readable.isEmpty() ? QString::fromUtf8("\xe2\x8f\xb3 0") : QString::fromUtf8("\xe2\x8f\xb3 %1").arg(readable);
    }

    inline QString formatDuration(int value, double fps = 0)
    {
        if (value <= 0)
            return QString::fromUtf8("\xe2\x8f\xb1 -");

        QString unit = getDurationUnit();
        double seconds = 0;
        if (unit == Output::DEFAULT_DELAY_IN_MILLISECONDS)
            seconds = value / 1000.0;
        else
        {
            if (fps <= 0) fps = 50.0;
            seconds = value / fps;
        }

        QString readable = formatSeconds(seconds);
        return readable.isEmpty() ? QString::fromUtf8("\xe2\x8f\xb1 -") : QString::fromUtf8("\xe2\x8f\xb1 %1").arg(readable);
    }

    inline double timecodeToSeconds(const QString& timecode)
    {
        // Parse "hh:mm:ss:ff" or "hh:mm:ss.ff" format.
        QStringList parts = timecode.split(QRegularExpression("[:.;]"));
        if (parts.count() < 3)
            return 0;
        return parts[0].toDouble() * 3600 + parts[1].toDouble() * 60 + parts[2].toDouble();
    }

    inline void configureBankOscSubscriptions(QWidget* widget, AbstractPlayoutCommand* playoutCommand, int bankId)
    {
        // Clean up old bank OscSubscriptions (tagged with objectName "bankOscSub").
        QList<OscSubscription*> oldSubs = widget->findChildren<OscSubscription*>("bankOscSub");
        qDeleteAll(oldSubs);

        if (bankId <= 0)
            return;

        // OscDeviceManager may not be initialized yet (Main.cpp initializes it after MainWindow
        // construction). If the listener is null, defer subscription creation until the event loop
        // starts, at which point OscDeviceManager::initialize() will have been called.
        if (OscDeviceManager::getInstance().getOscControlListener().isNull())
        {
            QTimer::singleShot(0, widget, [widget, playoutCommand, bankId]() {
                configureBankOscSubscriptions(widget, playoutCommand, bankId);
            });
            return;
        }

        QString uid = QString("bank%1").arg(bankId);

        auto createSub = [&](const QString& filterTemplate, Playout::PlayoutType type) {
            QString filter = filterTemplate;
            filter.replace("#UID#", uid);
            OscSubscription* sub = new OscSubscription(filter, widget);
            sub->setObjectName("bankOscSub");
            QObject::connect(sub, &OscSubscription::subscriptionReceived,
                [playoutCommand, widget, type, bankId](const QString&, const QList<QVariant>& arguments) {
                    if (arguments.count() > 0 && arguments[0].toInt() > 0)
                    {
                        // Notify RundownTreeWidget to deactivate the previous playing item
                        // and update its currentPlayingItem tracking.
                        TriggerBankRegistry::getInstance().fireBankTriggered(bankId);

                        // Activate the widget so the animation indicator fires inside executeCommand.
                        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(widget);
                        if (rw != nullptr)
                        {
                            if (isItemChannelLocked(rw))
                                return;

                            rw->setActive(true);
                            if (rw->getCommand() != nullptr)
                                rw->getCommand()->clearChannelOverride();
                        }

                        playoutCommand->executeCommand(type);
                        logPlayoutAction(rw, type);
                    }
                });
        };

        createSub(Osc::ITEM_CONTROL_STOP_FILTER, Playout::PlayoutType::Stop);
        createSub(Osc::ITEM_CONTROL_PLAY_FILTER, Playout::PlayoutType::Play);
        createSub(Osc::ITEM_CONTROL_PLAYNOW_FILTER, Playout::PlayoutType::PlayNow);
        createSub(Osc::ITEM_CONTROL_LOAD_FILTER, Playout::PlayoutType::Load);
        createSub(Osc::ITEM_CONTROL_PAUSE_FILTER, Playout::PlayoutType::PauseResume);
        createSub(Osc::ITEM_CONTROL_NEXT_FILTER, Playout::PlayoutType::Next);
        createSub(Osc::ITEM_CONTROL_CLEAR_FILTER, Playout::PlayoutType::Clear);
        createSub(Osc::ITEM_CONTROL_CLEARVIDEOLAYER_FILTER, Playout::PlayoutType::ClearVideoLayer);
        createSub(Osc::ITEM_CONTROL_CLEARCHANNEL_FILTER, Playout::PlayoutType::ClearChannel);
    }
}
