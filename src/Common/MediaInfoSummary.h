#pragma once

// One line about a media file for the Inspector - codec, resolution, frame rate,
// scan, alpha, audio and length - from what the server's media scanner found:
//
//     H.264 1920x1080 25p · AAC stereo 48 kHz · 2:35
//     PNG 1920x1080 · alpha
//
// The scanner answers GET /media/info/<name> with the ffprobe details it keeps
// for every file (the user's choice of source, 2026-09-23: the client reads no
// file itself). Each stream carries a codec object (long_name, sometimes name),
// width and height, pix_fmt, duration and nb_frames, channels and sample_rate;
// the file carries field_order and a format with its duration. There is no
// frame rate as such, so it is frames over duration.
//
// src/Widgets/MediaInfoClient asks; tools/test-mediainfo checks this.

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QtMath>

namespace MediaInfoSummary
{
    // ffprobe writes numbers as strings in some fields and as numbers in others.
    inline double number(const QJsonValue& value)
    {
        return value.isString() ? value.toString().toDouble() : value.toDouble();
    }

    // "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10" -> "H.264";
    // "PNG (Portable Network Graphics) image" -> "PNG"; "Motion JPEG" -> "JPEG" for
    // a still; "PCM signed 24-bit little-endian" -> "PCM 24-bit".
    inline QString codecName(const QJsonObject& stream, bool still)
    {
        const QJsonObject codec = stream.value("codec").toObject();
        QString name = codec.value("long_name").toString();
        if (name.isEmpty())
            name = stream.value("codec_long_name").toString();

        if (name.isEmpty())
        {
            const QString shortName = codec.value("name").toString(stream.value("codec_name").toString());
            return shortName.toUpper();
        }

        if (name.startsWith("PCM"))
        {
            static const QRegularExpression bits("(\\d+)-bit");
            const QRegularExpressionMatch match = bits.match(name);
            return match.hasMatch() ? QString("PCM %1-bit").arg(match.captured(1)) : QString("PCM");
        }

        name = name.section(" / ", 0, 0);
        const int bracket = name.indexOf(" (");
        if (bracket > 0)
            name = name.left(bracket);
        if (name.endsWith(" image"))
            name.chop(6);
        if (still && name == "Motion JPEG")
            name = "JPEG";

        return name.trimmed();
    }

    inline QString rateText(double fps)
    {
        static const double common[] = { 23.976, 24, 25, 29.97, 30, 48, 50, 59.94, 60, 100, 119.88, 120 };
        for (double rate : common)
        {
            if (qAbs(fps - rate) < 0.02)
                return QString::number(rate);
        }
        return QString::number(fps, 'f', 2).remove(QRegularExpression("\\.?0+$"));
    }

    inline bool hasAlpha(const QString& pixelFormat)
    {
        static const QRegularExpression alpha("^(yuva|gbrap|rgba|argb|bgra|abgr|ya\\d)");
        return alpha.match(pixelFormat).hasMatch();
    }

    inline QString lengthText(double seconds)
    {
        const qint64 total = qMax<qint64>(0, qRound64(seconds));
        const qint64 h = total / 3600;
        const qint64 m = (total % 3600) / 60;
        const qint64 s = total % 60;
        if (h > 0)
            return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }

    // The summary line, or empty when the answer holds no stream to describe.
    inline QString summarize(const QJsonObject& info, bool still)
    {
        const QJsonArray streams = info.value("streams").toArray();

        QJsonObject video;
        QList<QJsonObject> audio;
        for (const QJsonValue& value : streams)
        {
            const QJsonObject stream = value.toObject();
            const QString type = stream.value("codec").toObject().value("type").toString(stream.value("codec_type").toString());
            if (type == "video" && video.isEmpty())
                video = stream;
            else if (type == "audio")
                audio.append(stream);
        }

        QStringList parts;

        if (!video.isEmpty())
        {
            QString line = codecName(video, still);

            const int width = int(number(video.value("width")));
            const int height = int(number(video.value("height")));
            if (width > 0 && height > 0)
                line += QString(" %1x%2").arg(width).arg(height);

            if (!still)
            {
                const double frames = number(video.value("nb_frames"));
                double duration = number(video.value("duration"));
                if (duration <= 0)
                    duration = number(info.value("format").toObject().value("duration"));

                if (frames > 1 && duration > 0)
                {
                    const double fps = frames / duration;
                    const QString order = info.value("field_order").toString();
                    if (order == "tff" || order == "bff")
                        line += QString(" %1i").arg(rateText(fps * 2));   // fields a second: 1080 50i
                    else if (order == "progressive")
                        line += QString(" %1p").arg(rateText(fps));
                    else
                        line += QString(" %1 fps").arg(rateText(fps));
                }
            }

            parts.append(line.trimmed());

            if (hasAlpha(video.value("pix_fmt").toString()))
                parts.append("alpha");
        }

        if (!audio.isEmpty() && !still)
        {
            const QJsonObject first = audio.first();
            QString line = codecName(first, false);

            const int channels = int(number(first.value("channels")));
            if (channels == 1)
                line += " mono";
            else if (channels == 2)
                line += " stereo";
            else if (channels > 2)
                line += QString(" %1 ch").arg(channels);

            const double rate = number(first.value("sample_rate"));
            if (rate > 0)
                line += QString(" %1 kHz").arg(rateText(rate / 1000.0));

            if (audio.count() > 1)
                line += QString(" +%1 more").arg(audio.count() - 1);

            parts.append(line);
        }

        if (!still && !parts.isEmpty())
        {
            const double duration = number(info.value("format").toObject().value("duration"));
            if (duration > 0)
                parts.append(lengthText(duration));
        }

        return parts.join(QString::fromUtf8(" \xc2\xb7 "));
    }

    inline QString summarize(const QByteArray& json, bool still)
    {
        const QJsonDocument document = QJsonDocument::fromJson(json);
        return document.isObject() ? summarize(document.object(), still) : QString();
    }
}
