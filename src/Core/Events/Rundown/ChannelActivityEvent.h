#pragma once

#include "../../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT ChannelActivityEvent
{
    public:
        explicit ChannelActivityEvent(int channel, int videolayer,
                                      const QString& label,
                                      const QString& itemType,
                                      bool active);

        int getChannel() const;
        int getVideolayer() const;
        QString getLabel() const;
        QString getItemType() const;
        bool getActive() const;

    private:
        int channel = 0;
        int videolayer = 0;
        QString label;
        QString itemType;
        bool active = false;
};
