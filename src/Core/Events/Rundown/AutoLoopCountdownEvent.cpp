#include "AutoLoopCountdownEvent.h"

AutoLoopCountdownEvent::AutoLoopCountdownEvent(int channel, int videolayer,
                                               const QString& label,
                                               const QString& itemType,
                                               int remainingSeconds,
                                               int totalSeconds,
                                               bool active)
    : channel(channel), videolayer(videolayer), label(label), itemType(itemType),
      remainingSeconds(remainingSeconds), totalSeconds(totalSeconds), active(active)
{
}

int AutoLoopCountdownEvent::getChannel() const { return this->channel; }
int AutoLoopCountdownEvent::getVideolayer() const { return this->videolayer; }
QString AutoLoopCountdownEvent::getLabel() const { return this->label; }
QString AutoLoopCountdownEvent::getItemType() const { return this->itemType; }
int AutoLoopCountdownEvent::getRemainingSeconds() const { return this->remainingSeconds; }
int AutoLoopCountdownEvent::getTotalSeconds() const { return this->totalSeconds; }
bool AutoLoopCountdownEvent::getActive() const { return this->active; }
