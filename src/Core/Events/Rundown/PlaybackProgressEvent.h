#pragma once

#include "../../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT PlaybackProgressEvent
{
    public:
        explicit PlaybackProgressEvent(int channel, int videolayer,
                                       const QString& label,
                                       const QString& itemType,
                                       double time, double totalTime,
                                       double clip, double totalClip,
                                       double fps, bool paused, bool loop);

        int getChannel() const;
        int getVideolayer() const;
        QString getLabel() const;
        QString getItemType() const;
        double getTime() const;
        double getTotalTime() const;
        double getClip() const;
        double getTotalClip() const;
        double getFps() const;
        bool getPaused() const;
        bool getLoop() const;

    private:
        int channel;
        int videolayer;
        QString label;
        QString itemType;
        double time;
        double totalTime;
        double clip;
        double totalClip;
        double fps;
        bool paused;
        bool loop;
};
