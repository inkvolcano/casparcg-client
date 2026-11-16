#include "AppearanceSettings.h"

#include "DatabaseManager.h"
#include "Global.h"

void AppearanceSettings::load()
{
    auto value = [](const QString& key) { return DatabaseManager::getInstance().getConfigurationByName(key).getValue(); };

    ColorCache::setPvwButton(value("PVWButtonColor"));
    ColorCache::setStepButton(value("STEPButtonColor"));
    ColorCache::setPreviewBorder(value("PreviewBorderColor"));
    ColorCache::setAutostepHighlight(value("AutostepHighlightColor"));
    ColorCache::setActiveIndicator(value("ActiveIndicatorColor"));
    ColorCache::setLibrarySectionLine(value("LibrarySectionLineColor"));

    ColorCache::setHeaderLineMaster(value("HeaderLineMaster"));
    ColorCache::setHeaderBlockMaster(value("HeaderBlockMaster"));
    ColorCache::setHeaderTextMaster(value("HeaderTextMaster"));
    ColorCache::setHeaderLineRundown(value("HeaderLineRundown"));
    ColorCache::setHeaderBlockRundown(value("HeaderBlockRundown"));
    ColorCache::setHeaderTextRundown(value("HeaderTextRundown"));
    ColorCache::setCustomizeHeaders(value("CustomizeWidgetHeaders") == "true");

    static const char* panels[] = {
        "Library", "Inspector", "AudioLevels", "Preview", "Live", "Clock",
        "ServerStatus", "Activity", "TriggerBanks", "NDI", "Performance"
    };
    for (const char* panel : panels)
    {
        ColorCache::setLineOverride(panel, value(QString("HeaderLine_%1").arg(panel)));
        ColorCache::setBlockOverride(panel, value(QString("HeaderBlock_%1").arg(panel)));
        ColorCache::setTextOverride(panel, value(QString("HeaderText_%1").arg(panel)));
    }

    ColorCache::setClockColor1(value("ClockColor1"));
    ColorCache::setClockColor2(value("ClockColor2"));
    ColorCache::setClockShadow(value("ClockShadowColor"));

    auto number = [&value](const char* key, double fallback) {
        bool ok = false;
        const double v = value(key).toDouble(&ok);
        return ok ? v : fallback;
    };
    ChannelColor::setAngle(number("ChannelColorAngle", ChannelColor::DEFAULT_ANGLE));
    ChannelColor::setOffset(number("ChannelColorOffset", ChannelColor::DEFAULT_OFFSET));
    ChannelColor::setSaturation(number("ChannelColorSaturation", ChannelColor::DEFAULT_SATURATION));
    ChannelColor::setLightness(number("ChannelColorLightness", ChannelColor::DEFAULT_LIGHTNESS));
}
