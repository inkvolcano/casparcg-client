#pragma once

// The AMCP command that asks a server to send its output to the Live panel.
//
// The client does not decode anything here — it tells the *server* to encode and
// send a UDP mpegts stream, and then plays that. Which means the cost of the Live
// panel lands on the playout machine, not on the machine watching, and the
// parameters below are the whole of what decides that cost.
//
// Two things are open upstream about this, and both come back to this one string:
//
//   #271 (2019) — turning the Live preview on costs about 40% of an i7. The
//   reporter's diagnosis is the settings rather than the idea: libx264 at
//   ultrafast, encoding every frame at the channel's rate, to produce a 288x162
//   thumbnail. Nobody has changed them since.
//
//   #316 (2024) — the stream does not arrive at all with a 2.4.1 server, and VLC
//   on the same machine cannot open it either, while the 2.0.8 client works. The
//   reporter notes "unused option" warnings from the newer ffmpeg. A stream that
//   VLC also cannot open is a stream that was never produced properly, which
//   points at the options rather than at the receiving end.
//
// Neither can be fixed blind from here: both need a real server to test against,
// and guessing at ffmpeg options that somebody's working setup depends on is how
// you break the people it currently works for.
//
// So the parameters become editable, and the default is exactly the string the
// client has always sent. Nothing changes for anyone it works for, and anyone it
// does not work for — or costs too much — can now do something about it without
// waiting for a release.
//
// Header-only so the assembly can be tested without a server or a socket.

#include <QtCore/QList>
#include <QtCore/QString>

namespace StreamCommand
{
    // Everything after the URL, exactly as the client has always sent it.
    //
    // Kept character for character on purpose: this is the compatibility
    // baseline, and a test pins it so it cannot drift by accident.
    inline QString defaultParameters(int quality, bool key, int width, int height)
    {
        if (width > 0 && height > 0)
        {
            return QString("-format mpegts -codec:v libx264 -crf:v %1 -tune:v zerolatency "
                           "-preset:v ultrafast -filter:v %2scale=%3:%4  "
                           "-filter:a \"pan=stereo|c0=FL|c1=FR\"")
                .arg(quality)
                .arg(key ? "alphaextract,format=pix_fmts=yuv422p," : "")
                .arg(width)
                .arg(height);
        }

        return QString("-format mpegts -codec:v libx264 -crf:v %1 -tune:v zerolatency "
                       "-preset:v ultrafast %2  -filter:a \"pan=stereo|c0=FL|c1=FR\"")
            .arg(quality)
            .arg(key ? "-filter:v alphaextract" : "");
    }

    // What a custom string may stand in for. The tokens exist so a preset can
    // still follow the panel's own quality and size settings rather than
    // hard-coding them, which is the difference between a preset and a one-off.
    inline QString expand(const QString& parameters, int quality, bool key, int width, int height)
    {
        QString expanded = parameters;

        expanded.replace("{quality}", QString::number(quality));
        expanded.replace("{width}", QString::number(width > 0 ? width : 0));
        expanded.replace("{height}", QString::number(height > 0 ? height : 0));

        // Only meaningful with a key channel; empty otherwise, so a preset can
        // carry it unconditionally.
        expanded.replace("{key}", key ? "alphaextract,format=pix_fmts=yuv422p," : "");

        return expanded.trimmed();
    }

    // The parameters to actually send. An empty custom string means the built-in
    // one, which is what keeps the default behaviour untouched.
    inline QString parametersFor(const QString& custom, int quality, bool key, int width, int height)
    {
        const QString trimmed = custom.trimmed();

        if (trimmed.isEmpty())
            return defaultParameters(quality, key, width, height);

        return expand(trimmed, quality, key, width, height);
    }

    // The whole command. The server substitutes the client's address for the
    // placeholder, which is why it is sent literally.
    inline QString buildAdd(int channel, int port, const QString& parameters)
    {
        return QString("ADD %1 STREAM udp://<client_ip_address>:%2 %3")
            .arg(channel)
            .arg(port)
            .arg(parameters);
    }

    inline QString buildRemove(int channel, int port)
    {
        return QString("REMOVE %1 STREAM udp://<client_ip_address>:%2")
            .arg(channel)
            .arg(port);
    }

    // Starting points, offered in the settings tab. Each says what it trades,
    // because none of them can be verified from here and an operator picking one
    // should know what they are testing.
    struct Preset
    {
        QString name;
        QString parameters;
        QString note;
    };

    inline QList<Preset> presets()
    {
        QList<Preset> list;

        list.append({ "Default (as shipped)", QString(),
                      "Exactly what the client has always sent. Leave this if the Live panel works." });

        // The cost complaint in #271 is x264 doing real work per frame for a
        // thumbnail. mpeg2video is far cheaper per frame and nobody is judging
        // picture quality on a 288-pixel-wide preview.
        list.append({ "Lower server CPU (mpeg2video)",
                      "-format mpegts -codec:v mpeg2video -q:v 8 -filter:v {key}scale={width}:{height} "
                      "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
                      "Cheaper on the playout machine than x264. Try this first if enabling Live "
                      "costs you CPU you need." });

        // Halving the rate halves the encode, and a preview does not need 50.
        list.append({ "Lower server CPU (half frame rate)",
                      "-format mpegts -codec:v libx264 -crf:v {quality} -tune:v zerolatency "
                      "-preset:v ultrafast -r 25 -filter:v {key}scale={width}:{height} "
                      "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
                      "The same encoder at half the frames. A preview does not need the channel's "
                      "full rate." });

        // The #316 shape: no audio filter to fall over, and a packet size that
        // fits inside a normal MTU once headers are added.
        list.append({ "Video only, small packets",
                      "-format mpegts -codec:v libx264 -crf:v {quality} -tune:v zerolatency "
                      "-preset:v ultrafast -an -filter:v {key}scale={width}:{height}",
                      "No audio, and nothing exotic. Worth trying when the stream does not arrive "
                      "at all - if this works, the audio filter or the packet size was the problem." });

        return list;
    }
}
