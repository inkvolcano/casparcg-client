#pragma once

// How a channel number is coloured on a badge.
//
// The Activity panel and the Trigger Banks panel both draw one, and they have to
// agree: the same channel showing two different colours in two panels on the same
// screen is worse than either colour being wrong. They used to agree by being the
// same class. Now that they are not, the rule lives here rather than being copied
// into both, which is the way two copies start to differ.

#include "Global.h"

#include <QtCore/QString>
#include <QtGui/QColor>

namespace ChannelBadge
{
    // bigBold is the display setting both panels honour; it changes the text size
    // and nothing else about the colour.
    inline QString style(int channel, bool bigBold)
    {
        const QColor colour = QColor::fromHslF(ChannelColor::hue(channel) / 360.0,
                                               ChannelColor::saturation(),
                                               ChannelColor::lightness());

        return QString("background-color: %1; color: white; font-size: %2px; font-weight: bold;")
            .arg(colour.name())
            .arg(bigBold ? 14 : 10);
    }
}
