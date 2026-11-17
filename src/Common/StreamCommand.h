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
// So the parameters became editable (build 201), with the original string as
// the default.
//
// Build 318 measured it, against a 2.5 server on a 1080p25 channel (12-core
// machine, a clip looping, the server's CPU with and without the stream, three
// rounds each): the cost was the scaler, not x264. The default bicubic scale of
// every full-size frame down to a thumbnail cost about 6% of the machine;
// fast_bilinear halved it (about 3%). mpeg2video instead of x264 saved almost
// nothing. The Live window, which sent the full 1080p picture, cost about 13%
// and 6.5 Mbit/s; scaled to 1280 wide it is about 9% and 3.6 Mbit/s. Capping the
// rate at 25 costs nothing on a 25p channel and halves the frames on a 50p one.
// So the default changed to that, and the original string is a preset.
//
// Header-only so the assembly can be tested without a server or a socket.

#include <QtCore/QList>
#include <QtCore/QString>

namespace StreamCommand
{
    // Everything after the URL as the client sent it up to build 317. Kept
    // character for character (and pinned by a test) as the "Original" preset,
    // for a server that will not take the newer default.
    inline QString originalParameters(int quality, bool key, int width, int height)
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

    // The default since build 318: at most 25 frames a second, scaled with
    // fast_bilinear - the thumbnail to its size, the Live window to 1280 wide
    // with the channel's own shape.
    inline QString defaultParameters(int quality, bool key, int width, int height)
    {
        const QString keyFilter = key ? "alphaextract,format=pix_fmts=yuv422p," : "";
        const QString scale = (width > 0 && height > 0) ? QString("scale=%1:%2").arg(width).arg(height) : QString("scale=1280:-2");
        return QString("-format mpegts -codec:v libx264 -crf:v %1 -tune:v zerolatency -preset:v ultrafast "
                       "-filter:v fps=25,%2%3:flags=fast_bilinear -filter:a \"pan=stereo|c0=FL|c1=FR\"")
            .arg(quality)
            .arg(keyFilter)
            .arg(scale);
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

        list.append({ "Default", QString(),
                      "At most 25 frames a second with a cheap scaler: about half the server CPU of the "
                      "original string for the panel, a third less for the Live window (measured, build 318)." });

        // The string sent up to build 317. Scale 0:0 is ffmpeg for "the input's
        // own size", which is what the Live window always got.
        list.append({ "Original (up to build 317)",
                      "-format mpegts -codec:v libx264 -crf:v {quality} -tune:v zerolatency "
                      "-preset:v ultrafast -filter:v {key}scale={width}:{height} "
                      "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
                      "What the client always sent: every frame, bicubic scaling, the full picture in "
                      "the Live window. For a server that will not take the default." });

        // Half again, for a machine where every bit counts.
        list.append({ "Lowest server CPU (12.5 frames a second)",
                      "-format mpegts -codec:v libx264 -crf:v {quality} -tune:v zerolatency "
                      "-preset:v ultrafast -filter:v fps=12.5,{key}scale={width}:{height}:flags=fast_bilinear "
                      "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
                      "A choppier preview for less work. In the Live window it sends the full picture." });

        // Kept because it was offered; measured, it does not do what it promised.
        list.append({ "mpeg2video",
                      "-format mpegts -codec:v mpeg2video -q:v 8 -filter:v {key}scale={width}:{height} "
                      "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
                      "A different encoder. Measured on a 2.5 server it was hardly cheaper than x264 - "
                      "the scaling was the cost - so it is only worth trying if x264 itself is the problem." });

        // "Video only" (-an) is gone: on a 2.5 server the stream never arrived
        // and the consumer was dropped (measured, build 318).

        return list;
    }
}
