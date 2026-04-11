#include "ChannelActivityEvent.h"

ChannelActivityEvent::ChannelActivityEvent(int channel, int videolayer,
                                           const QString& label,
                                           const QString& itemType,
                                           bool active)
    : channel(channel), videolayer(videolayer), label(label), itemType(itemType), active(active)
{
}

int ChannelActivityEvent::getChannel() const
{
    return this->channel;
}

int ChannelActivityEvent::getVideolayer() const
{
    return this->videolayer;
}

QString ChannelActivityEvent::getLabel() const
{
    return this->label;
}

QString ChannelActivityEvent::getItemType() const
{
    return this->itemType;
}

bool ChannelActivityEvent::getActive() const
{
    return this->active;
}
