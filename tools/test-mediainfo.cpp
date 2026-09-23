// The Inspector's media line, from the scanner's media info.

#include "../src/Common/MediaInfoSummary.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expectEqual(const QString& actual, const QString& expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got      " << actual << "\n        expected " << expected << "\n";
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Media info\n";

    using MediaInfoSummary::summarize;
    const QString dot = QString::fromUtf8(" \xc2\xb7 ");

    const QByteArray h264 = R"json({
        "name": "AMB", "field_order": "progressive",
        "streams": [
            { "codec": { "long_name": "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10", "type": "video" },
              "width": 1920, "height": 1080, "pix_fmt": "yuv420p", "duration": "10.000000", "nb_frames": "250" },
            { "codec": { "long_name": "AAC (Advanced Audio Coding)", "type": "audio" },
              "channels": 2, "sample_rate": "48000" }
        ],
        "format": { "duration": "155.2" }
    })json";
    expectEqual(summarize(h264, false), "H.264 1920x1080 25p" + dot + "AAC stereo 48 kHz" + dot + "2:35", "an H.264 clip");

    const QByteArray interlaced = R"json({
        "field_order": "tff",
        "streams": [ { "codec": { "long_name": "Apple ProRes (iCodec Pro)", "type": "video" },
                       "width": 1920, "height": 1080, "pix_fmt": "yuva444p10le", "duration": "4", "nb_frames": "100" },
                     { "codec": { "long_name": "PCM signed 24-bit little-endian", "type": "audio" }, "channels": 8, "sample_rate": 48000 },
                     { "codec": { "long_name": "PCM signed 24-bit little-endian", "type": "audio" }, "channels": 2, "sample_rate": 48000 } ],
        "format": { "duration": 4 }
    })json";
    expectEqual(summarize(interlaced, false),
                "Apple ProRes 1920x1080 50i" + dot + "alpha" + dot + "PCM 24-bit 8 ch 48 kHz +1 more" + dot + "0:04",
                "an interlaced ProRes 4444 with alpha and two audio streams");

    const QByteArray ntsc = R"json({ "field_order": "progressive",
        "streams": [ { "codec": { "long_name": "HEVC (High Efficiency Video Coding)", "type": "video" },
                       "width": 3840, "height": 2160, "duration": "10.01", "nb_frames": "600" } ],
        "format": { "duration": "10.01" } })json";
    expectEqual(summarize(ntsc, false), "HEVC 3840x2160 59.94p" + dot + "0:10", "59.94 is named, and a clip without audio says nothing about audio");

    const QByteArray png = R"json({ "streams": [ { "codec": { "long_name": "PNG (Portable Network Graphics) image", "type": "video" },
                                            "width": 1920, "height": 1080, "pix_fmt": "rgba", "nb_frames": "1" } ] })json";
    expectEqual(summarize(png, true), "PNG 1920x1080" + dot + "alpha", "a PNG still with alpha");

    const QByteArray jpeg = R"json({ "streams": [ { "codec": { "long_name": "Motion JPEG", "type": "video" },
                                             "width": 3000, "height": 2000, "pix_fmt": "yuvj420p" } ] })json";
    expectEqual(summarize(jpeg, true), "JPEG 3000x2000", "a JPEG still is called JPEG");

    const QByteArray unknownOrder = R"json({ "streams": [ { "codec": { "long_name": "MPEG-2 video", "type": "video" },
                                   "width": 720, "height": 576, "duration": "2", "nb_frames": "50" } ] })json";
    expectEqual(summarize(unknownOrder, false), "MPEG-2 video 720x576 25 fps", "without a field order the rate is plain fps");

    const QByteArray flat = R"json({ "streams": [ { "codec_type": "video", "codec_name": "vp9", "width": 1280, "height": 720 } ] })json";
    expectEqual(summarize(flat, true), "VP9 1280x720", "a short codec name when there is no long one");

    // Verbatim answers from a real media-scanner (CasparCG Server 2.4 install).
    const QByteArray realMovie = R"json({"name":"010 INST RECAP SCHOOF","path":"C:\CasparCG\media\010 INST RECAP SCHOOF.mov","size":458676116,"time":1759414570000,"field_order":"unknown","streams":[{"codec":{"long_name":"Apple ProRes (iCodec Pro)","type":"video","tag_string":"apcn"},"width":1920,"height":1080,"sample_aspect_ratio":"1:1","display_aspect_ratio":"16:9","pix_fmt":"yuv422p10le","bits_per_raw_sample":"10","time_base":"1/25","start_time":"0.000000","duration_ts":750,"duration":"30.000000","bit_rate":"119730009","nb_frames":"750"},{"codec":{"long_name":"PCM signed 24-bit little-endian","type":"audio","tag_string":"in24"},"bits_per_raw_sample":"24","sample_fmt":"s32","sample_rate":"48000","channels":2,"channel_layout":"stereo","bits_per_sample":24,"time_base":"1/48000","start_time":"0.000000","duration_ts":1440000,"duration":"30.000000","bit_rate":"2304000","nb_frames":"1440000"},{"codec":{"type":"data","tag_string":"tmcd"},"time_base":"1/25","start_time":"0.000000","duration_ts":750,"duration":"30.000000","bit_rate":"1","nb_frames":"1"}],"format":{"name":"mov,mp4,m4a,3gp,3g2,mj2","long_name":"QuickTime / MOV","start_time":"0.000000","duration":"30.000000","bit_rate":"122313630"}})json";
    expectEqual(summarize(realMovie, false), "Apple ProRes 1920x1080 25 fps" + dot + "PCM 24-bit stereo 48 kHz" + dot + "0:30",
                "a real scanner answer for a ProRes clip, timecode track ignored");

    const QByteArray realStill = R"json({"name":"210 DLS ROEL","path":"C:\CasparCG\media\210 DLS ROEL.png","size":3145439,"time":1759412640000,"field_order":"unknown","streams":[{"codec":{"long_name":"PNG (Portable Network Graphics) image","type":"video","tag_string":"[0][0][0][0]"},"width":1920,"height":1080,"sample_aspect_ratio":"1:1","display_aspect_ratio":"16:9","pix_fmt":"rgba","time_base":"1/25"}],"format":{"name":"png_pipe","long_name":"piped png sequence"}})json";
    expectEqual(summarize(realStill, true), "PNG 1920x1080" + dot + "alpha", "a real scanner answer for a PNG still");

    expectEqual(summarize(QByteArray("{}"), false), "", "no streams: nothing");
    expectEqual(summarize(QByteArray("not json"), false), "", "not an answer: nothing");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
