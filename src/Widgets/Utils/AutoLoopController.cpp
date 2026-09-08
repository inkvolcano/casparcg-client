#include "AutoLoopController.h"

#include "EventManager.h"
#include "Events/Rundown/AutoLoopCountdownEvent.h"

AutoLoopController::AutoLoopController(QObject* parent)
    : QObject(parent)
{
    this->timer.setInterval(1000);
    this->timer.setSingleShot(false);
    QObject::connect(&this->timer, &QTimer::timeout, this, &AutoLoopController::onTick);
}

void AutoLoopController::setContext(int channel, int videolayer, const QString& label, const QString& itemType)
{
    this->channel = channel;
    this->videolayer = videolayer;
    this->label = label;
    this->itemType = itemType;
}

void AutoLoopController::setDelaySeconds(int delaySeconds)
{
    if (delaySeconds < 1)
        delaySeconds = 1;
    this->delaySeconds = delaySeconds;
}

int AutoLoopController::getDelaySeconds() const
{
    return this->delaySeconds;
}

void AutoLoopController::restartCountdown()
{
    this->active = true;
    this->remaining = this->delaySeconds;
    emitCountdown();
    this->timer.start();
}

void AutoLoopController::stop()
{
    if (!this->active && !this->timer.isActive())
        return;

    this->active = false;
    this->timer.stop();
    // Emit an inactive event so the Activity panel drops the row.
    EventManager::getInstance().fireAutoLoopCountdownEvent(
        AutoLoopCountdownEvent(this->channel, this->videolayer, this->label, this->itemType,
                               0, this->delaySeconds, false));
}

bool AutoLoopController::isActive() const
{
    return this->active;
}

int AutoLoopController::getRemainingSeconds() const
{
    return this->remaining;
}

void AutoLoopController::onTick()
{
    if (!this->active)
        return;

    this->remaining--;
    if (this->remaining > 0)
    {
        emitCountdown();
    }
    else
    {
        this->timer.stop();
        // Fire play; the caller's executePlay will call restartCountdown() to arm the next cycle.
        emit firePlay();
    }
}

void AutoLoopController::emitCountdown()
{
    EventManager::getInstance().fireAutoLoopCountdownEvent(
        AutoLoopCountdownEvent(this->channel, this->videolayer, this->label, this->itemType,
                               this->remaining, this->delaySeconds, true));
}
