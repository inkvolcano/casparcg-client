#pragma once

#include "../../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT AutoLoopCountdownEvent
{
    public:
        explicit AutoLoopCountdownEvent(int channel, int videolayer,
                                        const QString& label,
                                        const QString& itemType,
                                        int remainingSeconds,
                                        int totalSeconds,
                                        bool active);

        int getChannel() const;
        int getVideolayer() const;
        QString getLabel() const;
        QString getItemType() const;
        int getRemainingSeconds() const;
        int getTotalSeconds() const;
        bool getActive() const;

    private:
        int channel;
        int videolayer;
        QString label;
        QString itemType;
        int remainingSeconds;
        int totalSeconds;
        bool active;
};
