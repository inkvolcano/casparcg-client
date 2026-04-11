#include "PlaybackProgressEvent.h"

PlaybackProgressEvent::PlaybackProgressEvent(int channel, int videolayer,
                                             const QString& label,
                                             const QString& itemType,
                                             double time, double totalTime,
                                             double clip, double totalClip,
                                             double fps, bool paused, bool loop)
    : channel(channel), videolayer(videolayer), label(label), itemType(itemType),
      time(time), totalTime(totalTime), clip(clip), totalClip(totalClip),
      fps(fps), paused(paused), loop(loop)
{
}

int PlaybackProgressEvent::getChannel() const
{
    return this->channel;
}

int PlaybackProgressEvent::getVideolayer() const
{
    return this->videolayer;
}

QString PlaybackProgressEvent::getLabel() const
{
    return this->label;
}

QString PlaybackProgressEvent::getItemType() const
{
    return this->itemType;
}

double PlaybackProgressEvent::getTime() const
{
    return this->time;
}

double PlaybackProgressEvent::getTotalTime() const
{
    return this->totalTime;
}

double PlaybackProgressEvent::getClip() const
{
    return this->clip;
}

double PlaybackProgressEvent::getTotalClip() const
{
    return this->totalClip;
}

double PlaybackProgressEvent::getFps() const
{
    return this->fps;
}

bool PlaybackProgressEvent::getPaused() const
{
    return this->paused;
}

bool PlaybackProgressEvent::getLoop() const
{
    return this->loop;
}
