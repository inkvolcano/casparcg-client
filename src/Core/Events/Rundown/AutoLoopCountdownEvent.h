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
        int channel = 0;
        int videolayer = 0;
        QString label;
        QString itemType;
        int remainingSeconds = 0;
        int totalSeconds = 0;
        bool active = false;
};
