#pragma once

#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>

namespace Http
{
    static const QString DEFAULT_URL = "";
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Stream
{
    static const int DEFAULT_PORT = 9250;
    static const int COMPACT_WIDTH = 288;
    static const int COMPACT_HEIGHT = 162;
}

namespace Repository
{
    static const int DEFAULT_PORT = 8250;
}

namespace Osc
{
    static const bool DEFAULT_USE_BUNDLE = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const int DEFAULT_REFRESH_RATE = 200;
    static const int DEFAULT_MONITOR_PORT = 6250;
    static const int DEFAULT_CONTROL_PORT = 3250;
    static const int DEFAULT_WEBSOCKET_PORT = 4250;
    static const QString DEFAULT_OUTPUT = "";
    static const QString DEFAULT_TYPE = "String";
    static const int DEFAULT_OUTPUT_PORT = 7250;
    static const int DEFAULT_PROGRESSBAR_X = 64;
    static const int DEFAULT_PROGRESSBAR_Y = 3;
    static const int DEFAULT_PROGRESSBAR_HEIGHT = 24;
    static const int COMPACT_PROGRESSBAR_X = 64;
    static const int COMPACT_PROGRESSBAR_Y = 2;
    static const int COMPACT_PROGRESSBAR_HEIGHT = 11;
    static const int DEFAULT_LOOP_X = 180;
    static const int DEFAULT_LOOP_Y = 15;
    static const int COMPACT_LOOP_X = 180;
    static const int COMPACT_LOOP_Y = 0;
    static const int DEFAULT_PLAY_X = 102;
    static const int DEFAULT_PLAY_Y = 15;
    static const int COMPACT_PLAY_X = 102;
    static const int COMPACT_PLAY_Y = 0;
    static const int DEFAULT_PAUSE_X = 102;
    static const int DEFAULT_PAUSE_Y = 14;
    static const int COMPACT_PAUSE_X = 102;
    static const int COMPACT_PAUSE_Y = 0;
    static const QString DEFAULT_TIME = "00:00:00:00";
    static const QString FILERECORDER_FRAME_FILTER = "#IPADDRESS#/channel/#CHANNEL#/output/port/#PORT#/file/frame";
    static const QString FILERECORDER_FPS_FILTER = "#IPADDRESS#/channel/#CHANNEL#/output/port/#PORT#/file/fps";
    static const QString FILERECORDER_PATH_FILTER = "#IPADDRESS#/channel/#CHANNEL#/output/port/#PORT#/file/path";
    static const QString VIDEOLAYER_FPS_FILTER = "#IPADDRESS#/channel/#CHANNEL#/framerate";
    static const QString VIDEOLAYER_TIME_FILTER = "#IPADDRESS#/channel/#CHANNEL#/stage/layer/#VIDEOLAYER#/foreground/file/time";
    static const QString VIDEOLAYER_CLIP_FILTER = "#IPADDRESS#/channel/#CHANNEL#/stage/layer/#VIDEOLAYER#/foreground/file/clip";
    static const QString VIDEOLAYER_NAME_FILTER = "#IPADDRESS#/channel/#CHANNEL#/stage/layer/#VIDEOLAYER#/foreground/file/name";
    static const QString VIDEOLAYER_PAUSED_FILTER = "#IPADDRESS#/channel/#CHANNEL#/stage/layer/#VIDEOLAYER#/foreground/paused";
    static const QString VIDEOLAYER_LOOP_FILTER = "#IPADDRESS#/channel/#CHANNEL#/stage/layer/#VIDEOLAYER#/foreground/loop";
    static const QString AUDIOCHANNEL_FILTER = "#IPADDRESS#/channel/#CHANNEL#/mixer/audio/volume";
    static const QString ITEM_CONTROL_STOP_FILTER = "/control/#UID#/stop";
    static const QString ITEM_CONTROL_PLAY_FILTER = "/control/#UID#/play";
    static const QString ITEM_CONTROL_PLAYNOW_FILTER = "/control/#UID#/playnow";
    static const QString ITEM_CONTROL_LOAD_FILTER = "/control/#UID#/load";
    static const QString ITEM_CONTROL_PAUSE_FILTER = "/control/#UID#/pause";
    static const QString ITEM_CONTROL_NEXT_FILTER = "/control/#UID#/next";
    static const QString ITEM_CONTROL_UPDATE_FILTER = "/control/#UID#/update";
    static const QString ITEM_CONTROL_INVOKE_FILTER = "/control/#UID#/invoke";
    static const QString ITEM_CONTROL_PREVIEW_FILTER = "/control/#UID#/preview";
    static const QString ITEM_CONTROL_CLEAR_FILTER = "/control/#UID#/clear";
    static const QString ITEM_CONTROL_CLEARVIDEOLAYER_FILTER = "/control/#UID#/clearvideolayer";
    static const QString ITEM_CONTROL_CLEARCHANNEL_FILTER = "/control/#UID#/clearchannel";
    static const QString RUNDOWN_CONTROL_STOP_FILTER = "/control/stop";
    static const QString RUNDOWN_CONTROL_PLAY_FILTER = "/control/play";
    static const QString RUNDOWN_CONTROL_PLAYNOW_FILTER = "/control/playnow";
    static const QString RUNDOWN_CONTROL_LOAD_FILTER = "/control/load";
    static const QString RUNDOWN_CONTROL_PAUSE_FILTER = "/control/pause";
    static const QString RUNDOWN_CONTROL_NEXT_FILTER = "/control/next";
    static const QString RUNDOWN_CONTROL_UPDATE_FILTER = "/control/update";
    static const QString RUNDOWN_CONTROL_INVOKE_FILTER = "/control/invoke";
    static const QString RUNDOWN_CONTROL_PREVIEW_FILTER = "/control/preview";
    static const QString RUNDOWN_CONTROL_CLEAR_FILTER = "/control/clear";
    static const QString RUNDOWN_CONTROL_CLEARVIDEOLAYER_FILTER = "/control/clearvideolayer";
    static const QString RUNDOWN_CONTROL_CLEARCHANNELFILTER = "/control/clearchannel";
    static const QString RUNDOWN_CONTROL_DOWN_FILTER = "/control/down";
    static const QString RUNDOWN_CONTROL_UP_FILTER = "/control/up";
    static const QString RUNDOWN_CONTROL_PLAYNOWIFCHANNEL_FILTER = "/control/playnowifchannel";
}

namespace Route
{
    static const int DEFAULT_FROM_CHANNEL = 1;
    static const int DEFAULT_FROM_VIDEOLAYER = 1;
    static const int DEFAULT_OUTPUT_DELAY = 0;
}

namespace GpiOutput
{
    static const int DEFAULT_PORT = 0;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Mixer
{
    static const float DEFAULT_ANCHOR_XPOS = 0.0f;
    static const float DEFAULT_ANCHOR_YPOS = 0.0f;
    static const float DEFAULT_GRID = 2.0f;
    static const float DEFAULT_ROTATION = 0.0f;
    static const float DEFAULT_BRIGHTNESS = 1.0f;
    static const float DEFAULT_CONTRAST = 1.0f;
    static const float DEFAULT_CLIP_LEFT = 0.0f;
    static const float DEFAULT_CLIP_WIDTH = 1.0f;
    static const float DEFAULT_CLIP_TOP = 0.0f;
    static const float DEFAULT_CLIP_HEIGHT = 1.0f;
    static const float DEFAULT_FILL_XPOS = 0.0f;
    static const float DEFAULT_FILL_YPOS = 0.0f;
    static const float DEFAULT_FILL_XSCALE = 1.0f;
    static const float DEFAULT_FILL_YSCALE = 1.0f;
    static const float DEFAULT_LEVELS_MIN_IN = 0.0f;
    static const float DEFAULT_LEVELS_MAX_IN = 1.0f;
    static const float DEFAULT_LEVELS_MIN_OUT = 0.0f;
    static const float DEFAULT_LEVELS_MAX_OUT = 1.0f;
    static const float DEFAULT_LEVELS_GAMMA = 1.0f;
    static const float DEFAULT_OPACITY = 1.0f;
    static const float DEFAULT_SATURATION = 1.0f;
    static const float DEFAULT_VOLUME = 1.0f;
    static const float DEFAULT_CHROMABLENDSTART = 0.340f;
    static const float DEFAULT_CHROMABLENDSTOP = 0.440f;
    static const float DEFAULT_CHROMASPILL = 1.0f; 
    static const float DEFAULT_PERSPECTIVE_UPPERLEFT_X = 0.0f;
    static const float DEFAULT_PERSPECTIVE_UPPERLEFT_Y = 0.0f;
    static const float DEFAULT_PERSPECTIVE_UPPERRIGHT_X = 1.0f;
    static const float DEFAULT_PERSPECTIVE_UPPERRIGHT_Y = 0.0f;
    static const float DEFAULT_PERSPECTIVE_LOWERRIGHT_X = 1.0f;
    static const float DEFAULT_PERSPECTIVE_LOWERRIGHT_Y = 1.0f;
    static const float DEFAULT_PERSPECTIVE_LOWERLEFT_X = 0.0f;
    static const float DEFAULT_PERSPECTIVE_LOWERLEFT_Y = 1.0f;
    static const float DEFAULT_CROP_LEFT = 0.0f;
    static const float DEFAULT_CROP_TOP = 0.0f;
    static const float DEFAULT_CROP_RIGHT = 1.0f;
    static const float DEFAULT_CROP_BOTTOM = 1.0f;
    static const int DEFAULT_DURATION = 0;
    static const bool DEFAULT_DEFER = false;
    static const bool DEFAULT_MIPMAP = false;
    static const QString DEFAULT_BLENDMODE = "Normal";
    static const QString DEFAULT_TWEEN = "Linear";
    static const QString DEFAULT_DIRECTION = "RIGHT";
    static const QString DEFAULT_TRANSITION = "CUT";
    static const QString DEFAULT_CHROMAKEY = "None";
}

namespace Appearance
{
    static const QString CURVE_THEME = "Curve";
}

namespace Color
{
    static const QString DEFAULT_ACTIVE_COLOR = "rgba(0, 255, 0, 255)";           // Active indicator (bright green)
    static const QString DEFAULT_CONSUMER_COLOR = "rgba(25, 40, 110, 160)";       // Navy
    static const QString DEFAULT_GPI_COLOR = "rgba(220, 120, 30, 128)";           // Orange
    static const QString DEFAULT_GROUP_COLOR = "rgba(50, 100, 200, 128)";         // Blue
    static const QString DEFAULT_AUDIO_COLOR = "rgba(90, 105, 125, 120)";         // Cool Gray
    static const QString DEFAULT_STILL_COLOR = "rgba(200, 170, 40, 128)";         // Gold
    static const QString DEFAULT_MOVIE_COLOR = "rgba(70, 150, 210, 112)";         // Sky Blue
    static const QString DEFAULT_PRINT_COLOR = "rgba(200, 150, 30, 128)";         // Amber
    static const QString DEFAULT_CLEAR_OUTPUT_COLOR = "rgba(200, 85, 85, 100)";   // Soft Red
    static const QString DEFAULT_COLOR_PRODUCER_COLOR = "rgba(130, 60, 180, 128)";// Purple
    static const QString DEFAULT_MIXER_COLOR = "rgba(180, 80, 20, 140)";          // Burnt Orange
    static const QString DEFAULT_HTTP_COLOR = "rgba(110, 130, 40, 128)";          // Olive
    static const QString DEFAULT_PRODUCER_COLOR = "rgba(50, 160, 50, 128)";       // Green
    static const QString DEFAULT_TEMPLATE_COLOR = "rgba(40, 150, 140, 128)";      // Teal
    static const QString DEFAULT_SEPARATOR_COLOR = "rgba(150, 25, 25, 160)";      // Dark Red
    static const QString DEFAULT_AUTOPLAYGATEWAY_COLOR = "rgba(0, 150, 136, 128)"; // Teal
    static const QString DEFAULT_FOCUSGATEWAY_COLOR = "rgba(100, 80, 180, 128)";  // Purple
    static const QString DEFAULT_COMMANDGATEWAY_COLOR = "rgba(180, 100, 40, 128)"; // Burnt Orange
    static const QString DEFAULT_STORED_DATA_COLOR = "rgba(120, 70, 30, 140)";    // Brown
    static const QString DEFAULT_TRANSPARENT_COLOR = "Transparent";
    // Reds
    static const QString BRIGHT_RED_COLOR = "rgba(220, 50, 50, 128)";
    static const QString DARK_RED_COLOR = "rgba(150, 25, 25, 160)";
    static const QString SOFT_RED_COLOR = "rgba(200, 85, 85, 100)";
    static const QString ROSE_COLOR = "rgba(200, 60, 100, 128)";
    // Oranges
    static const QString ORANGE_COLOR = "rgba(220, 120, 30, 128)";
    static const QString BURNT_ORANGE_COLOR = "rgba(180, 80, 20, 140)";
    static const QString PEACH_COLOR = "rgba(220, 160, 100, 100)";
    // Yellows
    static const QString GOLD_COLOR = "rgba(200, 170, 40, 128)";
    static const QString YELLOW_COLOR = "rgba(210, 195, 50, 112)";
    static const QString AMBER_COLOR = "rgba(200, 150, 30, 128)";
    // Greens
    static const QString GREEN_COLOR = "rgba(50, 160, 50, 128)";
    static const QString DARK_GREEN_COLOR = "rgba(30, 110, 30, 150)";
    static const QString LIME_COLOR = "rgba(120, 185, 40, 120)";
    static const QString OLIVE_COLOR = "rgba(110, 130, 40, 128)";
    static const QString TEAL_COLOR = "rgba(40, 150, 140, 128)";
    static const QString MINT_COLOR = "rgba(80, 190, 140, 100)";
    // Blues
    static const QString BLUE_COLOR = "rgba(50, 100, 200, 128)";
    static const QString DARK_BLUE_COLOR = "rgba(30, 60, 150, 150)";
    static const QString LIGHT_BLUE_COLOR = "rgba(100, 165, 220, 100)";
    static const QString SKY_BLUE_COLOR = "rgba(70, 150, 210, 112)";
    static const QString NAVY_COLOR = "rgba(25, 40, 110, 160)";
    static const QString CYAN_COLOR = "rgba(40, 180, 200, 112)";
    // Purples
    static const QString PURPLE_COLOR = "rgba(130, 60, 180, 128)";
    static const QString DARK_PURPLE_COLOR = "rgba(80, 30, 130, 150)";
    static const QString LAVENDER_COLOR = "rgba(150, 120, 200, 100)";
    static const QString MAGENTA_COLOR = "rgba(180, 50, 150, 128)";
    // Neutrals & Browns
    static const QString BROWN_COLOR = "rgba(120, 70, 30, 140)";
    static const QString WARM_GRAY_COLOR = "rgba(140, 115, 100, 120)";
    static const QString COOL_GRAY_COLOR = "rgba(90, 105, 125, 120)";
    static const QString CHARCOAL_COLOR = "rgba(60, 60, 75, 160)";
}

namespace Stylesheet
{
    static const QString DEFAULT_BORDER_COLOR = "42, 42, 42, 255";
}

namespace TemplateData
{
    static const QString DEFAULT_COMPONENT_DATA_XML = "<componentData id=\\\"#KEY\\\"><data id=\\\"text\\\" value=\\\"#VALUE\\\"/></componentData>";
}

namespace Output
{
    static const int DEFAULT_CHANNEL = 1;
    static const int DEFAULT_VIDEOLAYER = 10;
    static const int DEFAULT_AUDIO_VIDEOLAYER = 30;
    static const int DEFAULT_FLASH_VIDEOLAYER = 20;
    static const int DEFAULT_DELAY = 0;
    static const int DEFAULT_DURATION = 0;
    static const bool DEFAULT_ALLOW_GPI = false;
    static const bool DEFAULT_ALLOW_REMOTE_TRIGGERING = false;
    static const QString DEFAULT_REMOTE_TRIGGER_ID = "";
    static const QString DEFAULT_DELAY_IN_FRAMES = "Frames";
    static const QString DEFAULT_DELAY_IN_MILLISECONDS = "Milliseconds";
    static const QString DEFAULT_PLAYOUT_COMMAND = "Play";
}

namespace Audio
{
    static const QString DEFAULT_NAME = "";
    static const bool DEFAULT_LOOP = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const bool DEFAULT_USE_AUTO = false;
}

namespace Movie
{
    static const QString DEFAULT_NAME = "";
    static const int DEFAULT_SEEK = 0;
    static const int DEFAULT_LENGTH = 0;
    static const bool DEFAULT_LOOP = false;
    static const bool DEFAULT_FREEZE_ON_LOAD = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const bool DEFAULT_AUTO_PLAY = false;
    static const bool DEFAULT_AUTO_LOOP = false;
    static const int DEFAULT_AUTO_LOOP_DELAY = 5;
}

namespace Html
{
    static const QString DEFAULT_URL = "";
    static const bool DEFAULT_FREEZE_ON_LOAD = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const bool DEFAULT_USE_AUTO = false;
}


namespace Still
{
    static const QString DEFAULT_NAME = "";
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const bool DEFAULT_USE_AUTO = false;
    static const bool DEFAULT_AUTO_PLAY = false;
    static const bool DEFAULT_AUTO_LOOP = false;
    static const int DEFAULT_AUTO_LOOP_DELAY = 5;
}

namespace ImageScroller
{
    static const QString DEFAULT_NAME = "";
    static const int DEFAULT_BLUR = 0;
    static const int DEFAULT_SPEED = 8;
    static const bool DEFAULT_PREMULTIPLY = false;
    static const bool DEFAULT_PROGRESSIVE = false;
}

namespace Opacity
{
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Fill
{
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Custom
{
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Rotation
{
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Anchor
{
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Template
{
    static const int DEFAULT_FLASHLAYER = 1;
    static const QString DEFAULT_INVOKE = "";
    static const QString DEFAULT_TEMPLATENAME = "";
    static const bool DEFAULT_USE_STORED_DATA = false;
    static const bool DEFAULT_USE_UPPERCASE_DATA = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const bool DEFAULT_SEND_AS_JSON = false;
    static const int DEFAULT_NEWLINE_BEHAVIOR = 2;  // 0=Ignore, 1=innerText, 2=innerHTML
    static const bool DEFAULT_AUTO_LOOP = false;
    static const int DEFAULT_AUTO_LOOP_DELAY = 5;
}

namespace DeckLinkInput
{
    static const int DEFAULT_DEVICE = 2;
    static const QString DEFAULT_FORMAT = "PAL";
}

namespace FileRecorder
{
    static const QString DEFAULT_OUTPUT = "Output.mxf";
    static const QString DEFAULT_DNXHD_CODEC = "dnxhd";
    static const QString DEFAULT_H264_CODEC = "h264";
    static const QString DEFAULT_DNXHD_PRESET = "-codec:v dnxhd -b:v 120M -flags:v +ilme+ildct -threads:v 4 -filter:v format=yuv422p,interlace";
    static const QString DEFAULT_H264_PRESET = "-codec:v libx264 -flags:v +ilme+ildct -threads:v 4 -filter:v interlace -preset:v veryfast";
    static const QString DEFAULT_CODEC = DEFAULT_DNXHD_CODEC;
    static const QString DEFAULT_PRESET = DEFAULT_DNXHD_PRESET;
    static const bool DEFAULT_WITH_ALPHA = false;
}

namespace SolidColor
{
    static const bool DEFAULT_USE_AUTO = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const QString DEFAULT_COLOR = "#FF000000";
}

namespace FadeToBlack
{
    static const bool DEFAULT_USE_AUTO = false;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
    static const QString DEFAULT_COLOR = "#00000000";
    static const int DEFAULT_DURATION = 12;
    static const QString DEFAULT_TRANSITION = "MIX";
}

namespace Library
{
    static const int TOOLS_PAGE_INDEX = 0;
    static const int AUDIO_PAGE_INDEX = 1;
    static const int STILL_PAGE_INDEX = 2;
    static const int TEMPLATE_PAGE_INDEX = 3;
    static const int MOVIE_PAGE_INDEX = 4;
    static const int DATA_PAGE_INDEX = 5;
    static const int PRESET_PAGE_INDEX = 6;
}

namespace Print
{
    static const QString DEFAULT_OUTPUT = "Snapshot";
}

namespace ClearOutput
{
    static const bool DEFAULT_CLEAR_CHANNEL = true;
    static const bool DEFAULT_TRIGGER_ON_NEXT = false;
}

namespace Group
{
    static const QString DEFAULT_NOTE = "";
    static const bool DEFAULT_AUTO_PLAY = false;
    static const bool DEFAULT_LOOP = false;
    static const bool DEFAULT_AUTO_LOOP = false;
    static const int DEFAULT_AUTO_LOOP_DELAY = 5;
}

namespace TriggerBank
{
    static const int BANK_COUNT = 9;
}

namespace ChannelColor
{
    // Configurable parameters with defaults.
    // Cached in statics to avoid per-frame DB reads.
    static const double DEFAULT_ANGLE = 137.508;
    static const double DEFAULT_OFFSET = 120.0;
    static const double DEFAULT_SATURATION = 0.65;
    static const double DEFAULT_LIGHTNESS = 0.30;

    inline double& cachedAngle()      { static double v = DEFAULT_ANGLE;      return v; }
    inline double& cachedOffset()     { static double v = DEFAULT_OFFSET;     return v; }
    inline double& cachedSaturation() { static double v = DEFAULT_SATURATION; return v; }
    inline double& cachedLightness()  { static double v = DEFAULT_LIGHTNESS;  return v; }

    inline double angle()      { return cachedAngle(); }
    inline double offset()     { return cachedOffset(); }
    inline double saturation() { return cachedSaturation(); }
    inline double lightness()  { return cachedLightness(); }

    inline void setAngle(double v)      { cachedAngle() = v; }
    inline void setOffset(double v)     { cachedOffset() = v; }
    inline void setSaturation(double v) { cachedSaturation() = v; }
    inline void setLightness(double v)  { cachedLightness() = v; }

    // Derived values for active indicator (brighter than badge).
    inline double activeSaturation() { return qMin(saturation() + 0.20, 1.0); }
    inline double activeLightness()  { return qMin(lightness() + 0.20, 0.50); }

    // Returns hue in degrees [0, 360).
    inline double hue(int channel)
    {
        double h = channel * angle() + offset();
        return h - static_cast<int>(h / 360.0) * 360.0;
    }
}

// Cached custom colors to avoid per-frame DB reads.
// Load once at startup via ColorCache::loadAll(), update from SettingsDialog on change.
namespace ColorCache
{
    inline QString& cachedActiveIndicator()     { static QString v; return v; }
    inline QString& cachedAutostepHighlight()   { static QString v; return v; }
    inline QString& cachedPreviewBorder()       { static QString v; return v; }
    inline QString& cachedPvwButton()           { static QString v; return v; }
    inline QString& cachedStepButton()          { static QString v; return v; }
    inline QString& cachedClockColor1()         { static QString v; return v; }
    inline QString& cachedClockColor2()         { static QString v; return v; }
    inline QString& cachedClockShadow()         { static QString v; return v; }
    inline QString& cachedLibrarySectionLine()  { static QString v; return v; }

    // Header colors: master, rundown override, per-widget overrides.
    inline QString& cachedHeaderLineMaster()    { static QString v; return v; }
    inline QString& cachedHeaderBlockMaster()   { static QString v; return v; }
    inline QString& cachedHeaderTextMaster()    { static QString v; return v; }
    inline QString& cachedHeaderLineRundown()   { static QString v; return v; }
    inline QString& cachedHeaderBlockRundown()  { static QString v; return v; }
    inline QString& cachedHeaderTextRundown()   { static QString v; return v; }
    inline bool& cachedCustomizeHeaders()       { static bool v = false; return v; }
    inline QHash<QString, QString>& cachedLineOverrides()  { static QHash<QString, QString> v; return v; }
    inline QHash<QString, QString>& cachedBlockOverrides() { static QHash<QString, QString> v; return v; }
    inline QHash<QString, QString>& cachedTextOverrides()  { static QHash<QString, QString> v; return v; }

    // Getters.
    inline const QString& activeIndicator()     { return cachedActiveIndicator(); }
    inline const QString& autostepHighlight()   { return cachedAutostepHighlight(); }
    inline const QString& previewBorder()       { return cachedPreviewBorder(); }
    inline const QString& pvwButton()           { return cachedPvwButton(); }
    inline const QString& stepButton()          { return cachedStepButton(); }
    inline const QString& clockColor1()         { return cachedClockColor1(); }
    inline const QString& clockColor2()         { return cachedClockColor2(); }
    inline const QString& clockShadow()         { return cachedClockShadow(); }
    inline const QString& librarySectionLine()  { return cachedLibrarySectionLine(); }

    inline const QString& headerLineMaster()    { return cachedHeaderLineMaster(); }
    inline const QString& headerBlockMaster()   { return cachedHeaderBlockMaster(); }
    inline const QString& headerTextMaster()    { return cachedHeaderTextMaster(); }
    inline const QString& headerLineRundown()   { return cachedHeaderLineRundown(); }
    inline const QString& headerBlockRundown()  { return cachedHeaderBlockRundown(); }
    inline const QString& headerTextRundown()   { return cachedHeaderTextRundown(); }
    inline bool customizeHeaders()              { return cachedCustomizeHeaders(); }
    inline QString lineOverride(const QString& panel)  { return cachedLineOverrides().value(panel); }
    inline QString blockOverride(const QString& panel) { return cachedBlockOverrides().value(panel); }
    inline QString textOverride(const QString& panel)  { return cachedTextOverrides().value(panel); }

    // Setters.
    inline void setActiveIndicator(const QString& v)     { cachedActiveIndicator() = v; }
    inline void setAutostepHighlight(const QString& v)   { cachedAutostepHighlight() = v; }
    inline void setPreviewBorder(const QString& v)       { cachedPreviewBorder() = v; }
    inline void setPvwButton(const QString& v)           { cachedPvwButton() = v; }
    inline void setStepButton(const QString& v)          { cachedStepButton() = v; }
    inline void setClockColor1(const QString& v)         { cachedClockColor1() = v; }
    inline void setClockColor2(const QString& v)         { cachedClockColor2() = v; }
    inline void setClockShadow(const QString& v)         { cachedClockShadow() = v; }
    inline void setLibrarySectionLine(const QString& v)  { cachedLibrarySectionLine() = v; }

    inline void setHeaderLineMaster(const QString& v)    { cachedHeaderLineMaster() = v; }
    inline void setHeaderBlockMaster(const QString& v)   { cachedHeaderBlockMaster() = v; }
    inline void setHeaderTextMaster(const QString& v)    { cachedHeaderTextMaster() = v; }
    inline void setHeaderLineRundown(const QString& v)   { cachedHeaderLineRundown() = v; }
    inline void setHeaderBlockRundown(const QString& v)  { cachedHeaderBlockRundown() = v; }
    inline void setHeaderTextRundown(const QString& v)   { cachedHeaderTextRundown() = v; }
    inline void setCustomizeHeaders(bool v)              { cachedCustomizeHeaders() = v; }
    inline void setLineOverride(const QString& panel, const QString& v)  { cachedLineOverrides()[panel] = v; }
    inline void setBlockOverride(const QString& panel, const QString& v) { cachedBlockOverrides()[panel] = v; }
    inline void setTextOverride(const QString& panel, const QString& v)  { cachedTextOverrides()[panel] = v; }
}

// Generates CSS override rules for widget header colors from ColorCache values.
namespace WidgetHeaderCSS
{
    inline QString generate()
    {
        QString css;

        // ── Master header colors (all panels) ────────────────────
        QString masterLine  = ColorCache::headerLineMaster();
        QString masterBlock = ColorCache::headerBlockMaster();
        QString masterText  = ColorCache::headerTextMaster();

        if (!masterLine.isEmpty())
            css += QString("QTabWidget::pane { border-top-color: %1; } "
                           "#treeWidgetRundown { border-top-color: %1; } ").arg(masterLine);
        if (!masterBlock.isEmpty() || !masterText.isEmpty())
        {
            css += "QTabBar::tab:selected { ";
            if (!masterBlock.isEmpty()) css += QString("background-color: %1; ").arg(masterBlock);
            if (!masterText.isEmpty())  css += QString("color: %1; ").arg(masterText);
            css += "} ";
        }

        // ── Rundown override (always applied when set) ───────────
        QString rundownLine  = ColorCache::headerLineRundown();
        QString rundownBlock = ColorCache::headerBlockRundown();
        QString rundownText  = ColorCache::headerTextRundown();

        static const char* rundownWidgets[] = {"tabWidgetRundown", "tabWidgetRundownSecondary"};
        for (const auto& w : rundownWidgets)
        {
            if (!rundownLine.isEmpty())
                css += QString("#%1::pane { border-top-color: %2; } ").arg(w, rundownLine);
            if (!rundownBlock.isEmpty() || !rundownText.isEmpty())
            {
                css += QString("#%1 QTabBar::tab:selected { ").arg(w);
                if (!rundownBlock.isEmpty()) css += QString("background-color: %1; ").arg(rundownBlock);
                if (!rundownText.isEmpty())  css += QString("color: %1; ").arg(rundownText);
                css += "} ";
            }
        }
        if (!rundownLine.isEmpty())
            css += QString("#treeWidgetRundown { border-top-color: %1; } ").arg(rundownLine);

        // ── Per-widget overrides (9 panels, when customization enabled) ──
        if (ColorCache::customizeHeaders())
        {
            struct P { const char* key; const char* widgets[2]; };
            static const P panels[] = {
                {"Library",      {"tabWidgetLibrary", nullptr}},
                {"Inspector",    {"tabWidgetInspector", nullptr}},
                {"AudioLevels",  {"tabWidgetAudioLevels", nullptr}},
                {"Preview",      {"tabWidgetPreview", nullptr}},
                {"Live",         {"tabWidgetLive", nullptr}},
                {"Clock",        {"tabWidgetClock", nullptr}},
                {"ServerStatus", {"tabWidgetServer", nullptr}},
                {"Activity",     {"tabWidgetActivity", nullptr}},
                {"TriggerBanks", {"tabWidgetBanks", nullptr}},
                {"NDI",          {"tabWidgetNdi", nullptr}},
                {"Performance",  {"tabWidgetPerformance", nullptr}},
            };
            for (const auto& p : panels)
            {
                QString line  = ColorCache::lineOverride(p.key);
                QString block = ColorCache::blockOverride(p.key);
                QString text  = ColorCache::textOverride(p.key);
                if (line.isEmpty() && block.isEmpty() && text.isEmpty()) continue;
                for (int i = 0; i < 2 && p.widgets[i]; ++i)
                {
                    if (!line.isEmpty())
                        css += QString("#%1::pane { border-top-color: %2; } ").arg(p.widgets[i], line);
                    if (!block.isEmpty() || !text.isEmpty())
                    {
                        css += QString("#%1 QTabBar::tab:selected { ").arg(p.widgets[i]);
                        if (!block.isEmpty()) css += QString("background-color: %1; ").arg(block);
                        if (!text.isEmpty())  css += QString("color: %1; ").arg(text);
                        css += "} ";
                    }
                }
            }
        }

        // Library section line (QToolBox open section indicator).
        QString sectionLine = ColorCache::librarySectionLine();
        if (!sectionLine.isEmpty())
            css += QString("QToolBox::tab:selected { border-bottom-color: %1; } ").arg(sectionLine);

        return css;
    }
}

namespace Rundown
{
    static const QString BLENDMODE = "BLENDMODE";
    static const QString BRIGHTNESS = "BRIGHTNESS";
    static const QString CONTRAST = "CONTRAST";
    static const QString CLIP = "CLIP";
    static const QString CROP = "CROP";
    static const QString IMAGESCROLLER = "IMAGESCROLLER";
    static const QString DECKLINKINPUT = "DECKLINKINPUT";
    static const QString PRINT = "PRINT";
    static const QString CLEAROUTPUT = "CLEAROUTPUT";
    static const QString FILL = "FILL";
    static const QString GPIOUTPUT = "GPIOUTPUT";
    static const QString HTTPGET = "HTTPGET";
    static const QString HTTPPOST = "HTTPPOST";
    static const QString FILERECORDER = "FILERECORDER";
    static const QString SEPARATOR = "SEPARATOR";
    static const QString GRID = "GRID";
    static const QString SOLIDCOLOR = "SOLIDCOLOR";
    static const QString KEYER = "KEYER";
    static const QString LEVELS = "LEVELS";
    static const QString OPACITY = "OPACITY";
    static const QString SATURATION = "SATURATION";
    static const QString VOLUME = "VOLUME";
    static const QString HTML = "HTML";
    static const QString COMMIT = "COMMIT";
    static const QString AUDIO = "AUDIO";
    static const QString STILL = "STILL";
    static const QString TEMPLATE = "TEMPLATE";
    static const QString MOVIE = "MOVIE";
    static const QString CUSTOMCOMMAND = "CUSTOMCOMMAND";
    static const QString PLAYOUTCOMMAND = "PLAYOUTCOMMAND";
    static const QString FADETOBLACK = "FADETOBLACK";
    static const QString CHROMAKEY = "CHROMAKEY";
    static const QString OSCOUTPUT = "OSCOUTPUT";
    static const QString PERSPECTIVE = "PERSPECTIVE";
    static const QString ROTATION = "ROTATION";
    static const QString RESET = "RESET";
    static const QString ANCHOR = "ANCHOR";
    static const QString ROUTECHANNEL = "ROUTECHANNEL";
    static const QString ROUTEVIDEOLAYER = "ROUTEVIDEOLAYER";
    static const QString AUTOPLAYGATEWAY = "AUTOPLAYGATEWAY";
    static const QString FOCUSGATEWAY = "FOCUSGATEWAY";
    static const QString COMMANDGATEWAY = "COMMANDGATEWAY";
    static const QString STOPAUTOLOOPS = "STOPAUTOLOOPS";
    static const int MAX_NUMBER_OF_RUNDONWS = 10;
    static const QString DEFAULT_NAME = "New Rundown";
    static const QString DEFAULT_AUDIO_NAME = "Audio";
    static const QString DEFAULT_STILL_NAME = "Image";
    static const QString DEFAULT_IMAGESCROLLER_NAME = "Image Scroller";
    static const QString DEFAULT_TEMPLATE_NAME = "Template";
    static const QString DEFAULT_MOVIE_NAME = "Video";
    static const int DEFAULT_ICON_WIDTH = 32;
    static const int DEFAULT_ICON_HEIGHT = 32;
    static const int COMPACT_ICON_WIDTH = 16;
    static const int COMPACT_ICON_HEIGHT = 16;
    static const int DEFAULT_ITEM_HEIGHT = 36;
    static const int COMPACT_ITEM_HEIGHT = 21;
    static const int DEFAULT_THUMBNAIL_WIDTH = 57;
    static const int DEFAULT_THUMBNAIL_HEIGHT = 32;
    static const int COMPACT_THUMBNAIL_WIDTH = 28;
    static const int COMPACT_THUMBNAIL_HEIGHT = 16;
    static const int GROUP_INDENTION = 65;
}

namespace Panel
{
    static const int DEFAULT_PREVIEW_HEIGHT = 188;
    static const int DEFAULT_LIVE_HEIGHT = 188;
    static const int DEFAULT_AUDIOLEVELS_HEIGHT = 147;
    static const int DEFAULT_CLOCK_HEIGHT = 58;
    static const int COMPACT_PREVIEW_HEIGHT = 25;
    static const int COMPACT_LIVE_HEIGHT = 25;
    static const int COMPACT_AUDIOLEVELS_HEIGHT = 25;
    static const int COMPACT_CLOCK_HEIGHT = 25;
    static const int DEFAULT_PERFORMANCE_HEIGHT = 110;
    static const int COMPACT_PERFORMANCE_HEIGHT = 25;
    static const int DEFAULT_HTTPLOG_HEIGHT = 200;
    static const int COMPACT_HTTPLOG_HEIGHT = 25;
    static const int DEFAULT_SHEETS_HEIGHT = 320;
    static const int COMPACT_SHEETS_HEIGHT = 25;
    static const int DEFAULT_SIMPLE_INSPECTOR_HEIGHT = 320;
}

namespace Action
{
    enum class ActionType
    {
        KeyPress,
        GpiPulse
    };
}

namespace XmlFormatting
{
    static const bool ENABLE_FORMATTING = true;
    static const int NUMBER_OF_SPACES = 4;
}

namespace Utils
{
    static const Qt::TimerType DEFAULT_TIMER_TYPE = Qt::PreciseTimer;
}
