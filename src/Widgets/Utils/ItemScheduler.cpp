#include "ItemScheduler.h"

#include "Global.h"
#include "DatabaseManager.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

ItemScheduler::ItemScheduler(QObject *parent) : QObject(parent)
{
    this->playTimer.setSingleShot(true);
    this->playTimer.setTimerType(Utils::DEFAULT_TIMER_TYPE);

    this->stopTimer.setSingleShot(true);
    this->stopTimer.setTimerType(Utils::DEFAULT_TIMER_TYPE);

    this->updateTimer.setSingleShot(true);
    this->updateTimer.setTimerType(Utils::DEFAULT_TIMER_TYPE);

    QObject::connect(&this->playTimer, SIGNAL(timeout()), SIGNAL(executePlay()));
    QObject::connect(&this->stopTimer, SIGNAL(timeout()), SIGNAL(executeStop()));
    QObject::connect(&this->updateTimer, SIGNAL(timeout()), SIGNAL(executeUpdate()));
}

int ItemScheduler::getMilliseconds(int frames, double framesPerSecond)
{
    return static_cast<int>(frames * (1000.0 / framesPerSecond));
}

void ItemScheduler::schedulePlayAndStop(int delay, int duration, const QString& /*delayType*/, int framesPerSecond)
{
    // Stop all timers.
    this->cancel();

    // Read units from DB for independent delay/duration unit support.
    QString delayUnit = DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();
    QString durationUnit = DatabaseManager::getInstance().getConfigurationByName("DurationUnit").getValue();

    int delayInMilliseconds = 0;
    if (delayUnit == Output::DEFAULT_DELAY_IN_FRAMES)
    {
        if (framesPerSecond > 0)
            delayInMilliseconds = getMilliseconds(delay, framesPerSecond);
        else
            qCritical("When delay type is frames, fps must be specified");
    }
    else
        delayInMilliseconds = delay;

    int durationInMilliseconds = 0;
    if (durationUnit == Output::DEFAULT_DELAY_IN_FRAMES)
    {
        if (framesPerSecond > 0)
            durationInMilliseconds = getMilliseconds(duration, framesPerSecond);
        else
            qCritical("When duration unit is frames, fps must be specified");
    }
    else
        durationInMilliseconds = duration;

    this->playTimer.setInterval(delayInMilliseconds);
    this->playTimer.start();

    if (durationInMilliseconds > 0)
    {
        this->stopTimer.setInterval(delayInMilliseconds + durationInMilliseconds);
        this->stopTimer.start();
    }
}

void ItemScheduler::scheduleUpdate(int delay, const QString& /*delayType*/, int framesPerSecond)
{
    this->updateTimer.stop();

    QString delayUnit = DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();

    int delayInMilliseconds = 0;
    if (delayUnit == Output::DEFAULT_DELAY_IN_FRAMES)
    {
        if (framesPerSecond > 0)
            delayInMilliseconds = getMilliseconds(delay, framesPerSecond);
        else
            qCritical("When delay type is frames, fps must be specified");
    }
    else
        delayInMilliseconds = delay;

    this->updateTimer.setInterval(delayInMilliseconds);
    this->updateTimer.start();
}

void ItemScheduler::cancel()
{
    this->playTimer.stop();
    this->stopTimer.stop();
    this->updateTimer.stop();
}
