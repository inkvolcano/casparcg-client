#pragma once

// Every setting CasparCG Server reads from casparcg.config, for the config
// editor: where it is, what kind of value, the default the server uses when it
// is left out, the values it accepts, and from which version it is read.
//
// Taken from the server's own reading code, not from the reference comment at
// the end of the shipped casparcg.config, which is wrong in places (it lists
// dci1080p*/dci2160p* video modes that do not exist - the real ones are
// 4kDCIp* - spells log-path's attribute "disable" where the server reads
// "disabled", and gives two wrong defaults). Checked against v2.4.3-stable,
// v2.5.1-stable and master fe19ac0 on 2026-10-20.
//
// check() predicts, without starting a server, what would stop it from
// starting and what would make it quietly skip a consumer.
//
// Tested by tools/test-serverconfig.

#include "ServerConfigXml.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>

namespace ServerConfigSchema
{
    using ServerConfigXml::NodePtr;

    enum class Version { V24 = 0, V25 = 1, Master = 2 };

    inline QString versionText(Version version)
    {
        switch (version)
        {
            case Version::V24: return "2.4";
            case Version::V25: return "2.5";
            default: return "2.6 / master";
        }
    }

    // From what the server reports ("2.5.0 7a5e4e2 Stable") or a folder name.
    // Anything newer than 2.5 reads what master reads; anything unrecognised is
    // treated as the newest, which shows every setting.
    inline Version versionFromText(const QString& text)
    {
        if (text.contains("master", Qt::CaseInsensitive))
            return Version::Master;

        const QRegularExpressionMatch m = QRegularExpression("(\\d+)\\.(\\d+)").match(text);
        if (!m.hasMatch())
            return Version::Master;

        const int major = m.captured(1).toInt();
        const int minor = m.captured(2).toInt();
        if (major < 2 || (major == 2 && minor <= 4))
            return Version::V24;
        if (major == 2 && minor == 5)
            return Version::V25;
        return Version::Master;
    }

    enum class Type
    {
        Bool,       // true / false
        Int,
        Float,
        Text,
        Path,       // a folder; relative is resolved against the server's folder
        Enum,
        VideoMode,  // a built-in mode or a custom one from <video-modes>
        Host,       // a name or an address
        IPv4,       // an address literal: the server does not look names up here
        Port,
        Timespan,   // frames, or a number with s, ms, us or ns
        Presence    // the element being there is the setting (decklink <vanc>)
    };

    struct Field
    {
        QString path;           // relative to the item: "a/b", "a/@x", "@x", "."
        Type type = Type::Text;
        QString defaultText;    // what the server uses when it is left out, as shown
        QStringList values;     // for Enum
        Version since = Version::V24;
        QString label;
        QString help;
        bool required = false;  // must be there for the server to start, or for the item to work
        bool keepGroups = false;
        bool caseSensitive = true;
        QString section;        // a heading the form groups it under
    };

    // The 41 built-in modes, spelled as the server's video_format.cpp spells
    // them. Looked up without regard to case.
    inline QStringList videoModes()
    {
        static const QStringList modes = QStringList()
            << "PAL" << "NTSC" << "576p2500"
            << "720p2398" << "720p2400" << "720p2500" << "720p2997" << "720p3000" << "720p5000" << "720p5994" << "720p6000"
            << "1080i5000" << "1080i5994" << "1080i6000"
            << "1080p2398" << "1080p2400" << "1080p2500" << "1080p2997" << "1080p3000" << "1080p5000" << "1080p5994" << "1080p6000"
            << "1556p2398" << "1556p2400" << "1556p2500"
            << "2160p2398" << "2160p2400" << "2160p2500" << "2160p2997" << "2160p3000" << "2160p5000" << "2160p5994" << "2160p6000"
            << "4kDCIp2398" << "4kDCIp2400" << "4kDCIp2500" << "4kDCIp2997" << "4kDCIp3000" << "4kDCIp5000" << "4kDCIp5994" << "4kDCIp6000";
        return modes;
    }

    // The consumers a channel can have in the config file, by element name.
    inline QStringList consumerKinds()
    {
        static const QStringList kinds = QStringList() << "screen" << "decklink" << "ndi" << "system-audio" << "ffmpeg" << "artnet" << "bluefish";
        return kinds;
    }

    inline QString consumerLabel(const QString& kind)
    {
        if (kind == "screen") return "Screen";
        if (kind == "decklink") return "DeckLink";
        if (kind == "ndi") return "NDI";
        if (kind == "system-audio") return "System audio";
        if (kind == "ffmpeg") return "FFmpeg (file or stream)";
        if (kind == "artnet") return "Art-Net (DMX lighting)";
        if (kind == "bluefish") return "Bluefish";
        return kind;
    }

    namespace Detail
    {
        inline Field field(const QString& path, Type type, const QString& defaultText, const QString& label,
                           const QString& help = QString(), Version since = Version::V24)
        {
            Field f;
            f.path = path;
            f.type = type;
            f.defaultText = defaultText;
            f.label = label;
            f.help = help;
            f.since = since;
            return f;
        }

        inline Field choice(const QString& path, const QStringList& values, const QString& defaultText, const QString& label,
                            const QString& help = QString(), Version since = Version::V24, bool caseSensitive = true)
        {
            Field f = field(path, Type::Enum, defaultText, label, help, since);
            f.values = values;
            f.caseSensitive = caseSensitive;
            return f;
        }

        inline Field required(Field f)
        {
            f.required = true;
            return f;
        }

        inline QList<Field> inSection(QList<Field> fields, const QString& section)
        {
            for (Field& f : fields)
                f.section = section;
            return fields;
        }

        inline QList<Field> subregion(const QString& prefix)
        {
            QList<Field> fields;
            fields << field(prefix + "subregion/src-x", Type::Int, "0", "Source x", "Offset into the channel.")
                   << field(prefix + "subregion/src-y", Type::Int, "0", "Source y")
                   << field(prefix + "subregion/dest-x", Type::Int, "0", "Output x", "Offset of the picture on the output.")
                   << field(prefix + "subregion/dest-y", Type::Int, "0", "Output y")
                   << field(prefix + "subregion/width", Type::Int, "0 (no limit)", "Width", "Width of the region copied. 0: no limit.")
                   << field(prefix + "subregion/height", Type::Int, "0 (no limit)", "Height");
            return inSection(fields, "Part of the channel (subregion)");
        }
    }

    // The settings of one kind of item. Pages of the whole file ("general",
    // "paths", ...) take paths from <configuration>; items ("channel", a
    // consumer, "producer", ...) from the item's own element.
    inline QList<Field> fieldsFor(const QString& kind)
    {
        using namespace Detail;
        QList<Field> f;

        if (kind == "general")
        {
            f << field("lock-clear-phrase", Type::Text, "(none)", "Lock clear phrase",
                       "What a client must send with LOCK n CLEAR. Left out, any client can clear a lock.")
              << choice("log-level", QStringList() << "trace" << "debug" << "info" << "warning" << "error" << "fatal", "info", "Log level",
                        "How much the server writes to its log.", Version::V24, false)
              << field("log-align-columns", Type::Bool, "true", "Align log columns")
              << choice("accelerator", QStringList() << "auto" << "opengl" << "vulkan", "auto (OpenGL)", "Accelerator",
                        "Only read by servers built with Vulkan support, which the published builds are not. Any other value "
                        "stops such a server.", Version::Master, false)
              << field("debugging/remote", Type::Bool, "false", "Remote debugging", "Only has an effect in debug builds of the server.");
        }
        else if (kind == "paths")
        {
            f << field("paths/media-path", Type::Path, "media/", "Media folder",
                       "Clips and stills. A relative path is inside the server's folder. Created if missing.")
              << field("paths/template-path", Type::Path, "template/", "Template folder", "HTML and Flash templates. Must be writable.")
              << field("paths/data-path", Type::Path, "data/", "Data folder", "Where DATA STORE keeps its files. Must be writable.")
              << required(field("paths/log-path", Type::Path, "log/", "Log folder",
                                "Must be in the file - the server does not start without a <log-path>. Must be writable."))
              << field("paths/log-path/@disabled", Type::Bool, "false", "Do not write log files",
                       "The attribute is \"disabled\". The shipped example's \"disable\" is not read.");
        }
        else if (kind == "scanner")
        {
            f << field("amcp/media-server/host", Type::Host, "127.0.0.1", "Scanner host",
                       "Where the server asks for media and template lists (CLS, TLS, CINF, thumbnails).")
              << field("amcp/media-server/port", Type::Port, "8000", "Scanner port");
        }
        else if (kind == "osc")
        {
            f << field("osc/default-port", Type::Port, "6250", "Port sent to on AMCP clients",
                       "OSC goes to this UDP port on the address of every connected AMCP client.")
              << field("osc/disable-send-to-amcp-clients", Type::Bool, "false", "Do not send OSC to AMCP clients");
        }
        else if (kind == "html")
        {
            f << field("html/enable-gpu", Type::Bool, "false", "Use the GPU for HTML")
              << choice("html/angle-backend", QStringList() << "gl" << "d3d11" << "d3d9" << "vulkan",
                        "Chromium chooses (gl in 2.4)", "ANGLE backend", "Only used when the GPU is on.")
              << field("html/remote-debugging-port", Type::Int, "0 (off)", "Remote debugging port",
                       "Open templates in Chrome's inspector at http://server:port. 0: off.")
              << field("html/cache-path", Type::Path, "cef-cache (none in 2.4)", "Cache folder",
                       "Where Chromium keeps its cache. Beside the server unless set.");
        }
        else if (kind == "ndi")
        {
            f << field("ndi/auto-load", Type::Bool, "false", "Load NDI at startup")
              << field("ndi/discovery-server", Type::Text, "(none)", "Discovery server",
                       "host:port (5959 if no port is given). Used by NDI outputs that use the advertiser.", Version::Master);
        }
        else if (kind == "ffmpeg")
        {
            f << field("ffmpeg/producer/threads", Type::Int, "0 (ffmpeg chooses)", "Decoding threads")
              << choice("ffmpeg/producer/auto-deinterlace", QStringList() << "none" << "interlaced" << "all", "interlaced",
                        "Deinterlace", "interlaced: only interlaced sources. all: everything. none: never.");
            f << inSection(QList<Field>()
                   << field("ffmpeg/producer/options/default", Type::Text, "(none)", "Every input", "\"-name value\" pairs, e.g. -rw_timeout 5000000", Version::Master)
                   << field("ffmpeg/producer/options/http", Type::Text, "(none)", "http and https", "e.g. -reconnect_delay_max 30", Version::Master)
                   << field("ffmpeg/producer/options/rtmp", Type::Text, "(none)", "rtmp and rtmps", QString(), Version::Master)
                   << field("ffmpeg/producer/options/srt", Type::Text, "(none)", "srt", QString(), Version::Master)
                   << field("ffmpeg/producer/options/udp", Type::Text, "(none)", "udp", QString(), Version::Master)
                   << field("ffmpeg/producer/options/rtsp", Type::Text, "(none)", "rtsp", QString(), Version::Master),
                   "Input options (an invalid value makes the clip fail to open)");
        }
        else if (kind == "audio")
        {
            f << field("system-audio/producer/default-device-name", Type::Text, "(the system's default)", "Audio device",
                       "The sound card system-audio outputs use. In 2.4 the only way to choose one; from 2.5 each output can name its own.");
        }
        else if (kind == "flash")
        {
            f << field("flash/enabled", Type::Bool, "false", "Flash templates", "Windows servers only.")
              << field("flash/buffer-depth", Type::Int, "2, or 4 above 30 fps", "Buffer depth");
        }
        else if (kind == "channel")
        {
            f << field("video-mode", Type::VideoMode, "PAL", "Video mode",
                       "An unknown or empty mode stops the server.")
              << choice("color-depth", QStringList() << "8" << "16", "8", "Colour depth",
                        "16 bit: NDI, Art-Net and Bluefish outputs are then skipped.", Version::V25)
              << choice("color-space", QStringList() << "bt709" << "bt2020", "bt709", "Colour space", QString(), Version::V25, false);
        }
        else if (kind == "screen")
        {
            f << field("name", Type::Text, "Screen consumer", "Window title")
              << field("device", Type::Int, "1", "Monitor", "1 is the first monitor.")
              << field("windowed", Type::Bool, "true", "Windowed")
              << field("borderless", Type::Bool, "false", "Borderless")
              << field("always-on-top", Type::Bool, "false", "Always on top")
              << field("interactive", Type::Bool, "true", "Interactive", "Mouse and keyboard reach HTML templates.")
              << field("vsync", Type::Bool, "false", "Vsync")
              << field("key-only", Type::Bool, "false", "Key only")
              << field("sbs-key", Type::Bool, "false", "Fill and key side by side")
              << choice("stretch", QStringList() << "none" << "fill" << "uniform" << "uniform_to_fill", "fill", "Stretch")
              << choice("aspect-ratio", QStringList() << "4:3" << "16:9", "from the video mode", "Aspect ratio")
              << choice("colour-space", QStringList() << "RGB" << "datavideo-full" << "datavideo-limited", "RGB", "Colour conversion",
                        "For DataVideo TC-100 and TC-200 scan converters.")
              << field("gpu-texture", Type::Bool, "false", "Share the GPU texture", QString(), Version::V25)
              << field("delay", Type::Timespan, "0", "Delay", "Frames (2), or a time (40ms). At most one second.", Version::V25);
            f << inSection(QList<Field>()
                   << field("x", Type::Int, "0", "x") << field("y", Type::Int, "0", "y")
                   << field("width", Type::Int, "0 (not set)", "Width") << field("height", Type::Int, "0 (not set)", "Height"),
                   "Window position and size");
        }
        else if (kind == "decklink")
        {
            f << field("device", Type::Int, "1", "Device", "1 is the first DeckLink output. From 2.5 also a card's persistent ID.")
              << field("key-only", Type::Bool, "false", "Key only")
              << field("video-mode", Type::VideoMode, "the channel's", "Video mode",
                       "Only if its frame rate is the channel's; otherwise the channel's mode is used. Custom modes are refused.")
              << choice("keyer", QStringList() << "external" << "external_separate_device" << "internal" << "disabled", "external", "Keyer",
                        "disabled: 2.5 and later. An unknown value is read as external.")
              << field("key-device", Type::Int, "device + 1", "Key device", "Only with the external_separate_device keyer.")
              << field("embedded-audio", Type::Bool, "false", "Embedded audio")
              << choice("latency", QStringList() << "normal" << "low", "normal", "Latency")
              << field("buffer-depth", Type::Int, "3", "Buffer depth")
              << choice("pixel-format", QStringList() << "rgba" << "yuv", "rgba (yuv on 16 bit)", "Pixel format",
                        "rgba on a 16-bit channel skips this output. yuv needs a CPU with AVX2.", Version::V25)
              << choice("color-space", QStringList() << "bt601" << "bt709" << "bt2020", "the channel's", "Colour space", QString(), Version::V25, false)
              << choice("duplex", QStringList() << "full" << "half", "the card's profile", "Duplex")
              << choice("wait-for-reference", QStringList() << "auto" << "enable" << "disable", "auto", "Wait for reference")
              << field("wait-for-reference-duration", Type::Int, "10", "Reference wait (seconds)");
            f << subregion(QString());
            f << inSection(QList<Field>()
                   << field("hdr-metadata/max-cll", Type::Float, "1000", "MaxCLL", QString(), Version::V25)
                   << field("hdr-metadata/max-fall", Type::Float, "100", "MaxFALL", QString(), Version::V25)
                   << field("hdr-metadata/min-dml", Type::Float, "0.005", "Min display luminance", QString(), Version::V25)
                   << field("hdr-metadata/max-dml", Type::Float, "1000", "Max display luminance", QString(), Version::V25),
                   "HDR metadata (on by itself on a 16-bit channel)");
            QList<Field> vanc;
            vanc << field("vanc", Type::Presence, "off", "VANC", "Any VANC setting, or this alone, turns VANC output on.", Version::Master);
            Field line = field("vanc/op47-line", Type::Int, "0 (off)", "OP-47 line", QString(), Version::Master);
            Field line2 = field("vanc/op47-line-field2", Type::Int, "0", "OP-47 line, field 2", QString(), Version::Master);
            Field sd = field("vanc/op42-sd-line", Type::Int, "21", "OP-42 SD line", QString(), Version::Master);
            Field scte = field("vanc/scte104-line", Type::Int, "0 (off)", "SCTE-104 line", QString(), Version::Master);
            Field header = field("vanc/op47-dummy-header", Type::Text, "(none)", "OP-47 dummy header", "Base64.", Version::Master);
            for (Field* x : {&line, &line2, &sd, &scte, &header})
                x->keepGroups = true;
            vanc << line << line2 << sd << scte << header;
            f << inSection(vanc, "VANC");
        }
        else if (kind == "port")
        {
            f << required(field("device", Type::Int, "(none)", "Device", "The DeckLink output this port drives, in sync with the main one."))
              << field("key-only", Type::Bool, "false", "Key only")
              << field("video-mode", Type::VideoMode, "the channel's", "Video mode");
            f << subregion(QString());
        }
        else if (kind == "ndi-output")
        {
            f << field("name", Type::Text, "CasparCG n", "Source name", "What receivers list.")
              << field("allow-fields", Type::Bool, "false", "Allow fields")
              << field("use-advertiser", Type::Bool, "false", "Use the advertiser", "Needs the NDI 5.5 runtime or newer.", Version::Master)
              << field("allow-monitoring", Type::Bool, "true", "Allow monitoring", "With the advertiser only.", Version::Master)
              << field("discovery-server", Type::Text, "the global one", "Discovery server", "host:port, with the advertiser only.", Version::Master);
        }
        else if (kind == "system-audio")
        {
            f << field("device-name", Type::Text, "the default device", "Audio device",
                       "An unknown name skips this output; the server logs the names it found.", Version::V25)
              << field("delay", Type::Timespan, "0", "Delay", "Frames (2), or a time (40ms).", Version::V25);
        }
        else if (kind == "ffmpeg-output")
        {
            f << required(field("path", Type::Text, "(none)", "File or URL",
                                "A file is put in the media folder unless the path is absolute. A file already there is DELETED when the server starts."))
              << field("args", Type::Text, "(none)", "Arguments", "ffmpeg-style, e.g. -codec:v libx264 -preset veryfast")
              << field("realtime", Type::Bool, "false", "Real time");
        }
        else if (kind == "artnet")
        {
            f << field("universe", Type::Int, "0", "Universe")
              << field("host", Type::IPv4, "127.0.0.1", "Address", "An IP address; a host name skips this output.")
              << field("port", Type::Port, "6454", "Port")
              << field("refresh-rate", Type::Int, "10", "Refresh rate", "At least 1.");
        }
        else if (kind == "fixture")
        {
            f << required(choice("type", QStringList() << "DIMMER" << "RGB" << "RGBW", "(none)", "Type", QString(), Version::V24, false))
              << required(field("start-address", Type::Int, "(none)", "Start address", "1 is the first DMX channel."))
              << required(field("fixture-count", Type::Int, "(none)", "Fixtures"))
              << field("fixture-channels", Type::Int, "the type's width", "Channels per fixture");
            f << inSection(QList<Field>()
                   << field("x", Type::Float, "0", "x") << field("y", Type::Float, "0", "y")
                   << field("width", Type::Float, "0", "Width") << field("height", Type::Float, "0", "Height")
                   << field("rotation", Type::Float, "0", "Rotation (degrees)"),
                   "Area sampled from the picture");
        }
        else if (kind == "bluefish")
        {
            f << field("device", Type::Int, "1", "Device")
              << choice("sdi-stream", QStringList() << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8", "1", "SDI stream")
              << field("embedded-audio", Type::Bool, "false", "Embedded audio")
              << choice("keyer", QStringList() << "disabled" << "external" << "internal", "disabled", "Keyer")
              << choice("internal-keyer-audio-source", QStringList() << "videooutputchannel" << "sdivideoinput", "videooutputchannel", "Internal keyer audio")
              << field("watchdog", Type::Int, "2", "Watchdog (frames)", "0 turns the bypass relay watchdog off.")
              << choice("uhd-mode", QStringList() << "0" << "1" << "2" << "3", "0", "UHD mode", "0 off, 1 auto, 2 force 2SI, 3 force square division.");
        }
        else if (kind == "producer")
        {
            f << required(field("@id", Type::Int, "(none)", "Layer", "Always written: a producer without it stops the server."))
              << required(field(".", Type::Text, "(none)", "Plays", "What follows PLAY channel-layer, e.g. AMB LOOP or DECKLINK DEVICE 2."));
        }
        else if (kind == "tcp")
        {
            f << required(field("port", Type::Port, "(none)", "Port", "A port already in use stops the server."))
              << required(choice("protocol", QStringList() << "AMCP", "(none)", "Protocol", QString(), Version::V24, false))
              << field("host", Type::IPv4, "every address", "Listen on", "An IPv4 address; a host name stops the server.", Version::Master);
        }
        else if (kind == "predefined-client")
        {
            f << required(field("address", Type::IPv4, "(none)", "Address"))
              << required(field("port", Type::Port, "(none)", "Port"));
        }
        else if (kind == "custom-mode")
        {
            f << required(field("id", Type::Text, "(none)", "Name", "Must not be a built-in mode's name."))
              << required(field("width", Type::Int, "(none)", "Width"))
              << required(field("height", Type::Int, "(none)", "Height"))
              << choice("field-count", QStringList() << "1" << "2", "1", "Fields", "2 for interlaced.")
              << field("time-scale", Type::Int, "60000", "Time scale", "Frame rate is time scale / duration.")
              << field("duration", Type::Int, "1000", "Duration")
              << field("cadence", Type::Text, "48000 / frame rate", "Audio cadence", "Samples per frame, e.g. 1602 1601 1602 1601 1602.");
        }
        else if (kind == "template-host")
        {
            f << field("video-mode", Type::VideoMode, "(any)", "Video mode", "Written exactly as the mode is spelled.")
              << field("filename", Type::Text, "cg.fth", "Template host file")
              << field("width", Type::Int, "the channel's", "Width")
              << field("height", Type::Int, "the channel's", "Height");
        }

        return f;
    }

    // Which field set an element of a channel's <consumers> uses.
    inline QString consumerFieldKind(const QString& element)
    {
        if (element == "ndi") return "ndi-output";
        if (element == "ffmpeg") return "ffmpeg-output";
        return element;
    }

    // The pages of the whole file, in the order the editor lists them.
    struct Page
    {
        QString kind;
        QString title;
    };

    inline QList<Page> pages()
    {
        return QList<Page>()
            << Page{"general", "General"}
            << Page{"paths", "Folders and logging"}
            << Page{"scanner", "Media scanner"}
            << Page{"osc", "OSC"}
            << Page{"html", "HTML templates"}
            << Page{"ffmpeg", "FFmpeg"}
            << Page{"ndi", "NDI"}
            << Page{"audio", "System audio"}
            << Page{"flash", "Flash (Windows)"};
    }

    // ---------------------------------------------------------------------
    // Values

    inline bool isTrue(const QString& text)
    {
        return text == "true" || text == "1";
    }

    inline bool isValidIPv4(const QString& text)
    {
        static const QRegularExpression ip("^(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}$");
        return ip.match(text.trimmed()).hasMatch();
    }

    inline bool isValidTimespan(const QString& text)
    {
        static const QRegularExpression span(QString::fromUtf8("^(\\d+|\\d+(\\.\\d+)?(s|ms|us|\xc2\xb5s|\xce\xbcs|ns))$"));
        return span.match(text.trimmed()).hasMatch();
    }

    // Whether a value is one the server takes as meant, or the reason it is not.
    // An empty value is never checked here: it means "left out".
    inline QString valueProblem(const Field& field, const QString& text, const QStringList& customModes)
    {
        const QString value = text.trimmed();
        if (value.isEmpty())
            return QString();

        bool ok = false;
        switch (field.type)
        {
            case Type::Bool:
                return (value == "true" || value == "false" || value == "1" || value == "0")
                    ? QString() : QString("\"%1\" is not true or false; the server uses %2.").arg(value, field.defaultText);
            case Type::Int:
                value.toLongLong(&ok);
                return ok ? QString() : QString("\"%1\" is not a whole number; the server uses %2.").arg(value, field.defaultText);
            case Type::Float:
                value.toDouble(&ok);
                return ok ? QString() : QString("\"%1\" is not a number; the server uses %2.").arg(value, field.defaultText);
            case Type::Port:
            {
                const int port = value.toInt(&ok);
                return ok && port > 0 && port <= 65535 ? QString() : QString("\"%1\" is not a port number (1-65535).").arg(value);
            }
            case Type::IPv4:
                return isValidIPv4(value) ? QString() : QString("\"%1\" is not an IPv4 address; this setting takes no host names.").arg(value);
            case Type::Timespan:
                return isValidTimespan(value) ? QString() : QString("\"%1\" is not a number of frames or a time like 40ms.").arg(value);
            case Type::Enum:
            {
                for (const QString& allowed : field.values)
                {
                    if (QString::compare(allowed, value, field.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0)
                        return QString();
                }
                return QString("\"%1\" is not one of %2.").arg(value, field.values.join(", "));
            }
            case Type::VideoMode:
            {
                QStringList all = videoModes();
                all << customModes;
                for (const QString& mode : all)
                {
                    if (QString::compare(mode, value, Qt::CaseInsensitive) == 0)
                        return QString();
                }
                if (value.startsWith("dci", Qt::CaseInsensitive))
                    return QString("\"%1\" is not a video mode. The example in the shipped config is wrong: the DCI modes are called 4kDCIp2500 and so on.").arg(value);
                return QString("\"%1\" is not a video mode.").arg(value);
            }
            default:
                return QString();
        }
    }

    // The ids of <video-modes>, which channels and template hosts may name.
    inline QStringList customModes(const NodePtr& root)
    {
        QStringList ids;
        for (const NodePtr& mode : ServerConfigXml::elements(ServerConfigXml::child(root, "video-modes"), "video-mode"))
        {
            const QString id = ServerConfigXml::value(mode, "id");
            if (!id.isEmpty())
                ids.append(id);
        }
        return ids;
    }

    // ---------------------------------------------------------------------
    // Items

    // What the editor's list calls an item.
    inline QString itemTitle(const NodePtr& item, const QString& kind, int number)
    {
        using ServerConfigXml::value;

        if (kind == "channel")
        {
            const QString mode = value(item, "video-mode");
            return QString("Channel %1  (%2)").arg(number).arg(mode.isEmpty() ? QString("PAL") : mode);
        }
        if (kind == "screen")
        {
            const QString device = value(item, "device");
            return QString("Screen %1%2").arg(device.isEmpty() ? QString("1") : device,
                                              isTrue(value(item, "key-only")) ? QString(" (key)") : QString());
        }
        if (kind == "decklink")
        {
            const QString device = value(item, "device");
            return QString("DeckLink %1").arg(device.isEmpty() ? QString("1") : device);
        }
        if (kind == "ndi")
        {
            const QString name = value(item, "name");
            return name.isEmpty() ? QString("NDI") : QString("NDI: %1").arg(name);
        }
        if (kind == "system-audio")
        {
            const QString device = value(item, "device-name");
            return device.isEmpty() ? QString("System audio") : QString("System audio: %1").arg(device);
        }
        if (kind == "ffmpeg")
        {
            const QString path = value(item, "path");
            return path.isEmpty() ? QString("FFmpeg") : QString("FFmpeg: %1").arg(path);
        }
        if (kind == "artnet")
            return QString("Art-Net universe %1").arg(value(item, "universe").isEmpty() ? QString("0") : value(item, "universe"));
        if (kind == "bluefish")
            return QString("Bluefish %1").arg(value(item, "device").isEmpty() ? QString("1") : value(item, "device"));
        if (kind == "producer")
            return QString("Layer %1: %2").arg(value(item, "@id"), value(item, "."));
        if (kind == "tcp")
            return QString("AMCP on port %1").arg(value(item, "port"));
        if (kind == "predefined-client")
            return QString("%1:%2").arg(value(item, "address"), value(item, "port"));
        if (kind == "custom-mode")
            return value(item, "id").isEmpty() ? QString("(no name)") : value(item, "id");
        if (kind == "template-host")
            return value(item, "video-mode").isEmpty() ? QString("Any video mode") : value(item, "video-mode");
        if (kind == "port")
            return QString("Port: device %1").arg(value(item, "device"));
        if (kind == "fixture")
            return QString("%1 x%2 from %3").arg(value(item, "type"), value(item, "fixture-count"), value(item, "start-address"));

        return QString("<%1>").arg(item->name);
    }

    // A new item, filled in enough to work.
    inline NodePtr newItem(const QString& kind, const NodePtr& root, const NodePtr& context = nullptr)
    {
        using namespace ServerConfigXml;

        if (kind == "channel")
        {
            NodePtr channel = makeElement("channel");
            // Most installations run every channel at one mode.
            QString mode = "1080i5000";
            const QList<NodePtr> channels = elements(child(root, "channels"), "channel");
            if (!channels.isEmpty() && !value(channels.last(), "video-mode").isEmpty())
                mode = value(channels.last(), "video-mode");
            appendChild(channel, makeElement("video-mode", mode));
            appendChild(channel, makeElement("consumers"));
            return channel;
        }
        if (kind == "screen")
            return makeElement("screen");
        if (kind == "decklink")
        {
            NodePtr node = makeElement("decklink");
            appendChild(node, makeElement("device", "1"));
            return node;
        }
        if (kind == "ndi")
        {
            NodePtr node = makeElement("ndi");
            int number = 1;
            if (context != nullptr)
            {
                const QList<NodePtr> channels = elements(child(root, "channels"), "channel");
                for (int i = 0; i < channels.count(); i++)
                {
                    if (channels.at(i).get() == context.get())
                        number = i + 1;
                }
            }
            appendChild(node, makeElement("name", QString("Channel %1").arg(number)));
            return node;
        }
        if (kind == "system-audio")
            return makeElement("system-audio");
        if (kind == "ffmpeg")
            return makeElement("ffmpeg");
        if (kind == "artnet")
        {
            NodePtr node = makeElement("artnet");
            appendChild(node, makeElement("fixtures"));   // required, even empty
            return node;
        }
        if (kind == "bluefish")
            return makeElement("bluefish");
        if (kind == "producer")
        {
            NodePtr node = makeElement("producer");
            int next = 10;
            for (const NodePtr& existing : elements(context != nullptr ? child(context, "producers") : nullptr, "producer"))
                next = qMax(next, attribute(existing, "id").toInt() + 10);
            setAttribute(node, "id", QString::number(next));
            return node;
        }
        if (kind == "tcp")
        {
            NodePtr node = makeElement("tcp");
            int port = 5250;
            for (const NodePtr& existing : elements(child(root, "controllers"), "tcp"))
                port = qMax(port, value(existing, "port").toInt() + 1);
            appendChild(node, makeElement("port", QString::number(port)));
            appendChild(node, makeElement("protocol", "AMCP"));
            return node;
        }
        if (kind == "predefined-client")
        {
            NodePtr node = makeElement("predefined-client");
            appendChild(node, makeElement("address", "127.0.0.1"));
            appendChild(node, makeElement("port", "5253"));
            return node;
        }
        if (kind == "custom-mode")
        {
            NodePtr node = makeElement("video-mode");
            appendChild(node, makeElement("id", "custom"));
            appendChild(node, makeElement("width", "1920"));
            appendChild(node, makeElement("height", "1080"));
            appendChild(node, makeElement("time-scale", "25000"));
            appendChild(node, makeElement("duration", "1000"));
            return node;
        }
        if (kind == "template-host")
        {
            NodePtr node = makeElement("template-host");
            appendChild(node, makeElement("filename", "cg.fth"));
            return node;
        }
        if (kind == "port")
        {
            NodePtr node = makeElement("port");
            appendChild(node, makeElement("device", "2"));
            return node;
        }
        if (kind == "fixture")
        {
            NodePtr node = makeElement("fixture");
            appendChild(node, makeElement("type", "RGB"));
            appendChild(node, makeElement("start-address", "1"));
            appendChild(node, makeElement("fixture-count", "1"));
            return node;
        }
        return makeElement(kind);
    }

    // A config the server starts with: one channel on screen, AMCP on 5250, the
    // scanner on 8000. For a server folder that has none.
    inline QString starterConfig()
    {
        return QString(
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<configuration>\n"
            "    <paths>\n"
            "        <media-path>media/</media-path>\n"
            "        <log-path>log/</log-path>\n"
            "        <data-path>data/</data-path>\n"
            "        <template-path>template/</template-path>\n"
            "    </paths>\n"
            "    <channels>\n"
            "        <channel>\n"
            "            <video-mode>1080i5000</video-mode>\n"
            "            <consumers>\n"
            "                <screen />\n"
            "                <system-audio />\n"
            "            </consumers>\n"
            "        </channel>\n"
            "    </channels>\n"
            "    <controllers>\n"
            "        <tcp>\n"
            "            <port>5250</port>\n"
            "            <protocol>AMCP</protocol>\n"
            "        </tcp>\n"
            "    </controllers>\n"
            "    <amcp>\n"
            "        <media-server>\n"
            "            <host>localhost</host>\n"
            "            <port>8000</port>\n"
            "        </media-server>\n"
            "    </amcp>\n"
            "</configuration>\n");
    }

    // ---------------------------------------------------------------------
    // Checks

    struct Problem
    {
        bool stopsServer = false;   // otherwise: one output skipped, or a setting ignored
        QString where;
        QString text;
    };

    namespace Detail
    {
        inline void checkFields(QList<Problem>& problems, const NodePtr& owner, const QString& kind, const QString& where,
                                const QStringList& custom, Version version, bool fatalValues)
        {
            for (const Field& field : fieldsFor(kind))
            {
                bool present = false;
                const QString text = ServerConfigXml::value(owner, field.path, &present);

                if (field.type == Type::Presence)
                    continue;

                if (present && text.isEmpty() && field.path != "." && !field.path.contains('@'))
                {
                    // An empty element is read as an empty string, not as the default.
                    const bool stops = fatalValues && field.path != "color-depth";
                    problems << Problem{stops, where, QString("%1 is in the file but empty, which the server does not read as \"%2\".")
                                                       .arg(field.label, field.defaultText)};
                    continue;
                }

                if (!present || text.isEmpty())
                {
                    if (field.required && field.since <= version)
                        problems << Problem{fatalValues || kind == "producer", where, QString("%1 is missing.").arg(field.label)};
                    continue;
                }

                if (field.since > version)
                {
                    problems << Problem{false, where, QString("%1 is read from version %2; a %3 server ignores it.")
                                                           .arg(field.label, versionText(field.since), versionText(version))};
                    continue;
                }

                const QString problem = valueProblem(field, text, custom);
                if (!problem.isEmpty())
                {
                    // Which wrong values stop the server and which fall back quietly.
                    const bool stops = (kind == "channel") || (kind == "tcp") || (kind == "predefined-client") || (kind == "custom-mode")
                                       || (kind == "general" && field.path == "accelerator" && version == Version::Master);
                    problems << Problem{stops, where, QString("%1: %2").arg(field.label, problem)};
                }
            }
        }

        inline QString consumerSlot(const NodePtr& consumer)
        {
            using ServerConfigXml::value;
            const QString name = consumer->name;
            if (name == "decklink")
                return "decklink device " + (value(consumer, "device").isEmpty() ? QString("1") : value(consumer, "device"));
            if (name == "bluefish")
                return "bluefish device " + (value(consumer, "device").isEmpty() ? QString("1") : value(consumer, "device"));
            if (name == "screen")
                return QString("screen %1 %2").arg(value(consumer, "device").isEmpty() ? QString("1") : value(consumer, "device"),
                                                    isTrue(value(consumer, "key-only")) ? QString("key") : QString("fill"));
            if (name == "ffmpeg")
                return "ffmpeg " + value(consumer, "path");
            return name;   // ndi, system-audio, artnet: one per channel
        }
    }

    // Everything that would stop the server or lose an output, worst first.
    inline QList<Problem> check(const ServerConfigXml::Document& document, Version version, bool windows = true)
    {
        using namespace ServerConfigXml;
        QList<Problem> problems;

        if (!document.ok)
        {
            problems << Problem{true, "File", QString("Not readable XML: %1 (line %2).").arg(document.error).arg(document.errorLine)};
            return problems;
        }

        const NodePtr root = document.root;
        if (root->name != "configuration")
        {
            problems << Problem{true, "File", QString("The outer element is <%1>, not <configuration>.").arg(root->name)};
            return problems;
        }

        const QStringList custom = customModes(root);

        for (const QString& needed : QStringList() << "paths" << "channels" << "controllers")
        {
            if (child(root, needed) == nullptr)
                problems << Problem{true, "File", QString("There is no <%1>; the server does not start without one.").arg(needed)};
        }
        if (child(root, "paths") != nullptr && find(root, "paths/log-path") == nullptr)
            problems << Problem{true, "Folders and logging", "There is no <log-path>; the server does not start without one."};
        if (hasAttribute(find(root, "paths/log-path"), "disable"))
            problems << Problem{false, "Folders and logging", "log-path has disable=\"...\", which the server does not read. The attribute is disabled=\"...\"."};

        for (const Page& page : pages())
            Detail::checkFields(problems, root, page.kind, page.title, custom, version, false);

        // Custom video modes.
        QSet<QString> seenModes;
        for (const QString& mode : videoModes())
            seenModes.insert(mode.toLower());
        for (const NodePtr& mode : elements(child(root, "video-modes")))
        {
            if (mode->name != "video-mode")
            {
                problems << Problem{true, "Custom video modes", QString("<%1> in <video-modes>: only <video-mode> is allowed.").arg(mode->name)};
                continue;
            }
            const QString id = value(mode, "id");
            const QString where = QString("Custom video mode %1").arg(id.isEmpty() ? QString("(no name)") : id);
            Detail::checkFields(problems, mode, "custom-mode", where, custom, version, true);
            if (!id.isEmpty())
            {
                if (seenModes.contains(id.toLower()))
                    problems << Problem{true, where, QString("\"%1\" is already a video mode.").arg(id)};
                seenModes.insert(id.toLower());
            }
            for (const QString& size : QStringList() << "width" << "height" << "time-scale" << "duration")
            {
                bool present = false;
                if (value(mode, size, &present) == "0" && present)
                    problems << Problem{true, where, QString("%1 cannot be 0.").arg(size)};
            }
        }

        // Channels.
        const QList<NodePtr> channelList = elements(child(root, "channels"));
        int number = 0;
        for (const NodePtr& channel : channelList)
        {
            number++;
            const QString where = QString("Channel %1").arg(number);
            if (channel->name != "channel")
            {
                problems << Problem{true, "Channels", QString("<%1> in <channels>: only <channel> is allowed.").arg(channel->name)};
                continue;
            }
            Detail::checkFields(problems, channel, "channel", where, custom, version, true);

            const bool deep = version >= Version::V25 && value(channel, "color-depth") == "16";
            QMap<QString, int> taken;
            for (const NodePtr& consumer : elements(child(channel, "consumers")))
            {
                const QString title = where + ", " + itemTitle(consumer, consumer->name, 0);
                if (!consumerKinds().contains(consumer->name))
                {
                    problems << Problem{false, title, QString("<%1> is not an output the server knows, so it is skipped.").arg(consumer->name)};
                    continue;
                }
                if (consumer->name == "bluefish" && !windows)
                    problems << Problem{false, title, "Bluefish outputs exist on Windows servers only."};

                Detail::checkFields(problems, consumer, consumerFieldKind(consumer->name), title, custom, version, false);

                const QString slot = Detail::consumerSlot(consumer);
                taken[slot]++;
                if (taken[slot] == 2)
                    problems << Problem{false, title, QString("A second %1 output on this channel replaces the first one.").arg(consumerLabel(consumer->name))};

                if (deep && (consumer->name == "ndi" || consumer->name == "artnet" || consumer->name == "bluefish"))
                    problems << Problem{false, title, "Skipped on a 16-bit channel: this output takes 8 bit only."};
                if (deep && consumer->name == "decklink" && value(consumer, "pixel-format") == "rgba")
                    problems << Problem{false, title, "rgba on a 16-bit channel: this output is skipped."};
                if (deep && consumer->name == "decklink" && isTrue(value(consumer, "key-only")))
                    problems << Problem{false, title, "Key only on a 16-bit channel: this output is skipped."};

                if (consumer->name == "decklink")
                {
                    const QString mode = value(consumer, "video-mode");
                    if (!mode.isEmpty() && custom.contains(mode, Qt::CaseInsensitive))
                        problems << Problem{false, title, "DeckLink cannot run a custom video mode: this output is skipped."};
                    for (const NodePtr& port : elements(child(consumer, "ports")))
                    {
                        if (port->name != "port")
                            problems << Problem{false, title, QString("<%1> in <ports>: only <port> is allowed, so this output is skipped.").arg(port->name)};
                        else
                            Detail::checkFields(problems, port, "port", title + ", " + itemTitle(port, "port", 0), custom, version, false);
                    }
                }

                if (consumer->name == "artnet")
                {
                    if (child(consumer, "fixtures") == nullptr)
                        problems << Problem{false, title, "There is no <fixtures>, which this output needs (it may be empty); it is skipped."};
                    bool rateOk = false;
                    const QString rate = value(consumer, "refresh-rate");
                    if (!rate.isEmpty() && rate.toInt(&rateOk) < 1 && rateOk)
                        problems << Problem{false, title, "Refresh rate below 1: this output is skipped."};

                    for (const NodePtr& fixture : elements(child(consumer, "fixtures")))
                    {
                        const QString fixtureWhere = title + ", " + itemTitle(fixture, "fixture", 0);
                        if (fixture->name != "fixture")
                        {
                            problems << Problem{false, title, QString("<%1> in <fixtures>: only <fixture> is allowed.").arg(fixture->name)};
                            continue;
                        }
                        Detail::checkFields(problems, fixture, "fixture", fixtureWhere, custom, version, false);

                        const QString type = value(fixture, "type").toUpper();
                        const int width = type == "DIMMER" ? 1 : type == "RGB" ? 3 : type == "RGBW" ? 4 : 0;
                        const int start = value(fixture, "start-address").toInt();
                        const int count = value(fixture, "fixture-count").toInt();
                        const QString channelsText = value(fixture, "fixture-channels");
                        const int channels = channelsText.isEmpty() ? width : channelsText.toInt();
                        if (width > 0 && channels < width)
                            problems << Problem{false, fixtureWhere, QString("Channels per fixture is below %1, the width of %2.").arg(width).arg(type)};
                        if (width > 0 && start >= 1 && count >= 1 && channels >= width)
                        {
                            const int last = start - 1 + (count - 1) * channels + width;
                            if (last > 512)
                                problems << Problem{true, fixtureWhere,
                                                    QString("Reaches DMX channel %1, past 512. The server does not check this and can crash.").arg(last)};
                        }
                    }
                }
            }

            QSet<QString> layers;
            for (const NodePtr& producer : elements(child(channel, "producers")))
            {
                if (producer->name != "producer")
                {
                    problems << Problem{true, where, QString("<%1> in <producers>: only <producer> is allowed.").arg(producer->name)};
                    continue;
                }
                const QString title = where + ", " + itemTitle(producer, "producer", 0);
                if (producer->attributes.isEmpty())
                {
                    problems << Problem{true, title, "A producer with no layer (id=\"...\") stops the server."};
                    continue;
                }
                Detail::checkFields(problems, producer, "producer", title, custom, version, false);
                const QString id = attribute(producer, "id");
                if (layers.contains(id))
                    problems << Problem{false, title, QString("A second producer on layer %1 replaces the first.").arg(id)};
                layers.insert(id);
            }
        }

        // Controllers.
        bool amcp = false;
        QSet<QString> ports;
        for (const NodePtr& controller : elements(child(root, "controllers")))
        {
            if (controller->name != "tcp")
            {
                problems << Problem{false, "Controllers", QString("<%1> is not a controller the server knows; it is ignored.").arg(controller->name)};
                continue;
            }
            const QString where = itemTitle(controller, "tcp", 0);
            Detail::checkFields(problems, controller, "tcp", where, custom, version, true);
            amcp = true;
            const QString port = value(controller, "port");
            if (!port.isEmpty() && ports.contains(port))
                problems << Problem{true, where, "Two controllers on the same port: the second cannot open it and the server does not start."};
            ports.insert(port);
        }
        if (child(root, "controllers") != nullptr && !amcp)
            problems << Problem{false, "Controllers", "No AMCP port: no client, this one included, can control the server."};

        // OSC clients.
        for (const NodePtr& client : elements(find(root, "osc/predefined-clients")))
        {
            if (client->name != "predefined-client")
            {
                problems << Problem{true, "OSC", QString("<%1> in <predefined-clients>: only <predefined-client> is allowed.").arg(client->name)};
                continue;
            }
            Detail::checkFields(problems, client, "predefined-client", "OSC client " + itemTitle(client, "predefined-client", 0), custom, version, true);
        }

        std::stable_sort(problems.begin(), problems.end(), [](const Problem& a, const Problem& b) { return a.stopsServer && !b.stopsServer; });
        return problems;
    }
}
