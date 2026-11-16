#pragma once

#include "Shared.h"

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

// Rundown items that fire at a time of day (build 303, Common/TimeTriggerRules.h).
//
// Once a second it walks every open rundown: an item with a "Fire at" time gets
// a badge on its row with the time and how long is left, and - only while
// Rundown > Arm Time Triggers is on - fires as if Play were pressed on it when
// its second comes. Armed is never saved: every start begins disarmed.
// Disabled items show their time but never fire.
class WIDGETS_EXPORT TimeTriggers : public QObject
{
    Q_OBJECT

    public:
        explicit TimeTriggers(QWidget* rundownArea, QObject* parent = nullptr);

        bool isArmed() const { return this->armed; }
        void setArmed(bool on);

    private:
        QPointer<QWidget> area;
        QTimer timer;
        QDateTime lastLook;
        bool armed = false;

        void tick();
};
