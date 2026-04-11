#include "ClockWidget.h"

#include "Global.h"
#include "PanelHelper.h"

#include "DatabaseManager.h"

#include "Models/ConfigurationModel.h"

#include <QtCore/QDateTime>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

static QString gmtOffsetString(const QTimeZone& tz, const QDateTime& utcNow)
{
    int secs = tz.offsetFromUtc(utcNow);
    if (secs == 0) return "UTC";
    QString sign = (secs > 0) ? "+" : "-";
    int absSecs = qAbs(secs);
    int h = absSecs / 3600;
    int m = (absSecs % 3600) / 60;
    if (m > 0)
        return QString("GMT%1%2:%3").arg(sign).arg(h).arg(m, 2, 10, QChar('0'));
    return QString("GMT%1%2").arg(sign).arg(h);
}

static QLCDNumber* makeLcd(int digits, QWidget* parent, int x, int y, int w, int h,
                           const QString& color)
{
    QLCDNumber* lcd = new QLCDNumber(digits, parent);
    lcd->setGeometry(x, y, w, h);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->setFrameShape(QFrame::NoFrame);
    lcd->setStyleSheet(QString("color: %1;").arg(color));
    return lcd;
}

static QLabel* makeColon(QWidget* parent, int x, int y, int w, int h,
                         const QString& color)
{
    QLabel* colon = new QLabel(":", parent);
    colon->setGeometry(x, y, w, h);
    colon->setAlignment(Qt::AlignCenter);
    colon->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(color));
    return colon;
}

QFrame* ClockWidget::createClockCard(QLCDNumber*& lcdH, QLCDNumber*& lcdM, QLCDNumber*& lcdS,
                                      QLCDNumber*& shadowH, QLCDNumber*& shadowM, QLCDNumber*& shadowS,
                                      QLabel*& colonA, QLabel*& colonB,
                                      const QString& lcdColor, const QString& shadowColor)
{
    // Layout: [HH]:[MM]:[SS] with zero-gap colons.
    const int FRAME_W = 130;
    const int FRAME_H = 52;
    const int LCD_Y = 4;
    const int LCD_H = 44;
    const int LCD_W = 40;
    const int COLON_W = 2;
    const int SHADOW_H = 36;
    const int SHADOW_Y = 8;

    const int X_H = 3;
    const int X_C1 = X_H + LCD_W;         // 43
    const int X_M = X_C1 + COLON_W;       // 51
    const int X_C2 = X_M + LCD_W;         // 91
    const int X_S = X_C2 + COLON_W;       // 99

    QFrame* frame = new QFrame(this->widgetClockContent);
    frame->setFixedSize(FRAME_W, FRAME_H);

    // Shadows (behind everything).
    shadowH = makeLcd(2, frame, X_H, SHADOW_Y, LCD_W, SHADOW_H, shadowColor);
    shadowH->display(88);
    shadowM = makeLcd(2, frame, X_M, SHADOW_Y, LCD_W, SHADOW_H, shadowColor);
    shadowM->display(88);
    shadowS = makeLcd(2, frame, X_S, SHADOW_Y, LCD_W, SHADOW_H, shadowColor);
    shadowS->display(88);

    // Colon labels (between shadows and main LCDs in z-order).
    colonA = makeColon(frame, X_C1, LCD_Y, COLON_W, LCD_H, lcdColor);
    colonB = makeColon(frame, X_C2, LCD_Y, COLON_W, LCD_H, lcdColor);

    // Main digit LCDs (on top).
    lcdH = makeLcd(2, frame, X_H, LCD_Y, LCD_W, LCD_H, lcdColor);
    lcdH->display("00");
    lcdM = makeLcd(2, frame, X_M, LCD_Y, LCD_W, LCD_H, lcdColor);
    lcdM->display("00");
    lcdS = makeLcd(2, frame, X_S, LCD_Y, LCD_W, LCD_H, lcdColor);
    lcdS->display("00");

    // Z-order.
    shadowH->lower();
    shadowM->lower();
    shadowS->lower();
    lcdH->raise();
    lcdM->raise();
    lcdS->raise();

    return frame;
}

ClockWidget::ClockWidget(QWidget* parent)
    : QWidget(parent),
      collapsed(false)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Clock");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    // Read timezone settings from DB.
    QString tz1 = DatabaseManager::getInstance()
        .getConfigurationByName("ClockTimezone1").getValue();
    QString tz2 = DatabaseManager::getInstance()
        .getConfigurationByName("ClockTimezone2").getValue();
    QString dualStr = DatabaseManager::getInstance()
        .getConfigurationByName("ClockDualMode").getValue();

    this->timezone1 = (tz1.isEmpty() || tz1 == "Local")
        ? QTimeZone::systemTimeZone() : QTimeZone(tz1.toUtf8());
    this->timezone2 = (tz2.isEmpty() || tz2 == "UTC")
        ? QTimeZone::utc() : QTimeZone(tz2.toUtf8());
    this->dualClock = (dualStr.isEmpty() || dualStr == "true");

    // Read color settings from cache (populated by SettingsDialog at startup).
    QString c1 = ColorCache::clockColor1();
    QString c2 = ColorCache::clockColor2();
    QString cs = ColorCache::clockShadow();
    this->color1 = c1.isEmpty() ? "rgba(220, 220, 220, 230)" : c1;
    this->color2 = c2.isEmpty() ? "rgba(80, 200, 200, 220)" : c2;
    this->shadowColor = cs.isEmpty() ? "rgba(55, 55, 55, 255)" : cs;

    // Read layout settings from DB.
    QString showLabelsStr = DatabaseManager::getInstance().getConfigurationByName("ClockShowLabels").getValue();
    this->showLabels = (showLabelsStr.isEmpty() || showLabelsStr == "true");
    QString stackedStr = DatabaseManager::getInstance().getConfigurationByName("ClockStacked").getValue();
    this->stacked = (stackedStr == "true");

    // Card 1 — primary timezone (wrapped in container for layout management).
    this->card1Widget = new QWidget(this->widgetClockContent);
    QVBoxLayout* card1 = new QVBoxLayout(this->card1Widget);
    card1->setSpacing(0);
    card1->setContentsMargins(0, 0, 0, 0);

    this->zoneLabel1 = new QLabel(this->card1Widget);
    this->zoneLabel1->setAlignment(Qt::AlignCenter);
    this->zoneLabel1->setStyleSheet("font-size: 12px; color: rgba(140, 140, 140, 200);");
    card1->addWidget(this->zoneLabel1);

    this->clockFrame1 = createClockCard(this->lcd1H, this->lcd1M, this->lcd1S,
        this->shadow1Hour, this->shadow1Minute, this->shadow1Second,
        this->colon1a, this->colon1b,
        this->color1, this->shadowColor);
    card1->addWidget(this->clockFrame1, 0, Qt::AlignCenter);

    // Vertical divider.
    this->divider = new QFrame(this->widgetClockContent);
    this->divider->setFrameShape(QFrame::VLine);
    this->divider->setStyleSheet("color: rgba(60, 60, 60, 180);");
    this->divider->setFixedWidth(1);

    // Card 2 — secondary timezone (wrapped in a container for easy show/hide).
    this->card2Widget = new QWidget(this->widgetClockContent);
    QVBoxLayout* card2 = new QVBoxLayout(this->card2Widget);
    card2->setSpacing(0);
    card2->setContentsMargins(0, 0, 0, 0);

    this->zoneLabel2 = new QLabel(this->card2Widget);
    this->zoneLabel2->setAlignment(Qt::AlignCenter);
    this->zoneLabel2->setStyleSheet("font-size: 12px; color: rgba(140, 140, 140, 200);");
    card2->addWidget(this->zoneLabel2);

    this->clockFrame2 = createClockCard(this->lcd2H, this->lcd2M, this->lcd2S,
        this->shadow2Hour, this->shadow2Minute, this->shadow2Second,
        this->colon2a, this->colon2b,
        this->color2, this->shadowColor);
    card2->addWidget(this->clockFrame2, 0, Qt::AlignCenter);

    // Apply label visibility.
    this->zoneLabel1->setVisible(this->showLabels);
    this->zoneLabel2->setVisible(this->showLabels);

    // Build initial layout.
    rebuildLayout();

    // Timer — update every second.
    this->clockTimer = new QTimer(this);
    QObject::connect(this->clockTimer, &QTimer::timeout, this, &ClockWidget::updateClocks);
    this->clockTimer->start(1000);
    updateClocks();
}

void ClockWidget::rebuildLayout()
{
    // Remove old layout from widgetClockContent.
    QLayout* oldLayout = this->widgetClockContent->layout();
    if (oldLayout)
    {
        // Remove all items without deleting widgets.
        while (oldLayout->count())
            oldLayout->takeAt(0);
        delete oldLayout;
    }

    if (this->stacked)
    {
        // Stacked: card1 on top, card2 below, no divider.
        QVBoxLayout* layout = new QVBoxLayout(this->widgetClockContent);
        layout->setContentsMargins(1, 0, 1, 0);
        layout->setSpacing(0);
        layout->addWidget(this->card1Widget, 0, Qt::AlignCenter);
        layout->addWidget(this->card2Widget, 0, Qt::AlignCenter);
        this->divider->hide();
    }
    else
    {
        // Side-by-side: card1 | divider | card2.
        QHBoxLayout* layout = new QHBoxLayout(this->widgetClockContent);
        layout->setContentsMargins(1, 0, 1, 0);
        layout->setSpacing(0);
        layout->addWidget(this->card1Widget, 1);
        layout->addWidget(this->divider);
        layout->addWidget(this->card2Widget, 1);
        this->divider->setVisible(this->dualClock);
    }

    // Show/hide second clock.
    this->card1Widget->show();
    this->card2Widget->setVisible(this->dualClock);

    // Store the calculated height so MainWindow::rebuildLayout() can use it
    // instead of the static Panel::DEFAULT_CLOCK_HEIGHT (which is too small).
    this->setProperty("panelFixedHeight", expandedHeight());

    if (!this->collapsed)
        this->setFixedHeight(expandedHeight());
}

QMenu* ClockWidget::createTimezoneMenu(const QString& title, int clockIndex)
{
    QMenu* menu = new QMenu(title, this);

    QAction* localAction = menu->addAction("Local");
    QObject::connect(localAction, &QAction::triggered, this, [this, clockIndex]() {
        setTimezone(clockIndex, QTimeZone::systemTimeZone(), "Local");
    });

    menu->addSeparator();

    for (int offset = -12; offset <= 14; offset++)
    {
        QString label = (offset == 0) ? "GMT 0"
                      : (offset > 0) ? QString("GMT +%1").arg(offset)
                      : QString("GMT %1").arg(offset);

        QByteArray tzId = QString("UTC%1%2:00")
            .arg(offset >= 0 ? "+" : "")
            .arg(offset, 2, 10, QChar('0')).toUtf8();

        QTimeZone tz(tzId);
        if (!tz.isValid())
            tz = QTimeZone(QString("Etc/GMT%1%2").arg(offset <= 0 ? "+" : "-").arg(qAbs(offset)).toUtf8());

        QObject::connect(menu->addAction(label), &QAction::triggered, this, [this, clockIndex, tz, tzId]() {
            setTimezone(clockIndex, tz, QString::fromUtf8(tzId));
        });
    }

    return menu;
}

void ClockWidget::setTimezone(int clockIndex, const QTimeZone& tz, const QString& dbValue)
{
    if (clockIndex == 1)
    {
        this->timezone1 = tz;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockTimezone1", dbValue));
    }
    else
    {
        this->timezone2 = tz;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockTimezone2", dbValue));
    }

    updateClocks();
}

void ClockWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Clock", this);
    this->dropdownMenu->addSeparator();
    this->dropdownMenu->addMenu(createTimezoneMenu("Clock 1 Timezone", 1));
    this->dropdownMenu->addMenu(createTimezoneMenu("Clock 2 Timezone", 2));
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &ClockWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetClock);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetClock->setCornerWidget(this->menuButton);
}

void ClockWidget::updateClocks()
{
    QDateTime now = QDateTime::currentDateTimeUtc();

    QTime t1 = now.toTimeZone(this->timezone1).time();
    this->lcd1H->display(QString("%1").arg(t1.hour(), 2, 10, QChar('0')));
    this->lcd1M->display(QString("%1").arg(t1.minute(), 2, 10, QChar('0')));
    this->lcd1S->display(QString("%1").arg(t1.second(), 2, 10, QChar('0')));

    QTime t2 = now.toTimeZone(this->timezone2).time();
    this->lcd2H->display(QString("%1").arg(t2.hour(), 2, 10, QChar('0')));
    this->lcd2M->display(QString("%1").arg(t2.minute(), 2, 10, QChar('0')));
    this->lcd2S->display(QString("%1").arg(t2.second(), 2, 10, QChar('0')));

    this->zoneLabel1->setText(gmtOffsetString(this->timezone1, now));
    this->zoneLabel2->setText(gmtOffsetString(this->timezone2, now));
}

int ClockWidget::expandedHeight() const
{
    int h = Panel::COMPACT_CLOCK_HEIGHT; // tab bar
    h += 52; // card1 frame
    if (this->showLabels) h += 14;
    if (this->dualClock && this->stacked)
    {
        h += 52; // card2 frame
        if (this->showLabels) h += 14;
    }
    return h + 2; // margins
}

void ClockWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Clock", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_CLOCK_HEIGHT);
    else
        PanelHelper::applyExpandedHeight(this, "Clock", expandedHeight());
}
