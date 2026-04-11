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
        bool collapsed;
        bool dualClock;
        bool stacked;
        bool showLabels;
        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;
        QFrame* divider;
        QWidget* card2Widget;
        QWidget* card1Widget;

        QLabel* zoneLabel1;
        QLabel* zoneLabel2;

        QString color1;
        QString color2;
        QString shadowColor;

        // Clock 1 — separate LCDs per digit pair.
        QFrame* clockFrame1;
        QLCDNumber* lcd1H;
        QLCDNumber* lcd1M;
        QLCDNumber* lcd1S;
        QLCDNumber* shadow1Hour;
        QLCDNumber* shadow1Minute;
        QLCDNumber* shadow1Second;

        // Clock 2 — separate LCDs per digit pair.
        QFrame* clockFrame2;
        QLCDNumber* lcd2H;
        QLCDNumber* lcd2M;
        QLCDNumber* lcd2S;
        QLCDNumber* shadow2Hour;
        QLCDNumber* shadow2Minute;
        QLCDNumber* shadow2Second;

        QTimer* clockTimer;
        QTimeZone timezone1;
        QTimeZone timezone2;

        // Colon labels (for color updates).
        QLabel* colon1a;
        QLabel* colon1b;
        QLabel* colon2a;
        QLabel* colon2b;

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
