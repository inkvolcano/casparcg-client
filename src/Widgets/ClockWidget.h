#pragma once

#include "Shared.h"
#include "ui_ClockWidget.h"

#include <QtCore/QTimeZone>
#include <QtCore/QTimer>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT ClockWidget : public QWidget, Ui::ClockWidget
{
    Q_OBJECT

    public:
        explicit ClockWidget(QWidget* parent = 0);

    private:
        bool collapsed = false;
        bool dualClock = false;
        bool stacked = false;
        bool showLabels = false;
        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;
        QFrame* divider = nullptr;
        QWidget* card2Widget = nullptr;
        QWidget* card1Widget = nullptr;

        QLabel* zoneLabel1 = nullptr;
        QLabel* zoneLabel2 = nullptr;

        QString color1;
        QString color2;
        QString shadowColor;

        // Clock 1 — separate LCDs per digit pair.
        QFrame* clockFrame1 = nullptr;
        QLCDNumber* lcd1H = nullptr;
        QLCDNumber* lcd1M = nullptr;
        QLCDNumber* lcd1S = nullptr;
        QLCDNumber* shadow1Hour = nullptr;
        QLCDNumber* shadow1Minute = nullptr;
        QLCDNumber* shadow1Second = nullptr;

        // Clock 2 — separate LCDs per digit pair.
        QFrame* clockFrame2 = nullptr;
        QLCDNumber* lcd2H = nullptr;
        QLCDNumber* lcd2M = nullptr;
        QLCDNumber* lcd2S = nullptr;
        QLCDNumber* shadow2Hour = nullptr;
        QLCDNumber* shadow2Minute = nullptr;
        QLCDNumber* shadow2Second = nullptr;

        QTimer* clockTimer = nullptr;
        QTimeZone timezone1;
        QTimeZone timezone2;

        // Colon labels (for color updates).
        QLabel* colon1a = nullptr;
        QLabel* colon1b = nullptr;
        QLabel* colon2a = nullptr;
        QLabel* colon2b = nullptr;

        void setupMenus();
        void updateClocks();
        int expandedHeight() const;
        void rebuildLayout();
        QFrame* createClockCard(QLCDNumber*& lcdH, QLCDNumber*& lcdM, QLCDNumber*& lcdS,
                                QLCDNumber*& shadowH, QLCDNumber*& shadowM, QLCDNumber*& shadowS,
                                QLabel*& colonA, QLabel*& colonB,
                                const QString& lcdColor, const QString& shadowColor);
        QMenu* createTimezoneMenu(const QString& title, int clockIndex);
        void setTimezone(int clockIndex, const QTimeZone& tz, const QString& dbValue);

        Q_SLOT void toggleExpandCollapse();
};
