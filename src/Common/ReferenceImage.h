#pragma once

// A reference image on a rundown item: a picture of what the item puts on air, so
// an operator can tell lower thirds apart by hovering instead of by name.
//
// The user's choices (2026-10-13): kept inside the rundown file, added from the
// Inspector or by dropping an image file on a row, shown when hovering the row,
// with a small icon on rows that have one.
//
// Stored as a data URI ("data:image/jpeg;base64,...") so the rundown carries it
// to any machine and a tooltip can show it as it is. Scaled to fit 480 x 480 -
// enough to recognise a graphic, about 30-60 KB a picture - and kept as JPEG,
// or PNG when the picture has transparency, which JPEG would fill in.
//
// tools/test-referenceimage checks it.

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QImage>

namespace ReferenceImage
{
    static const int MAX_SIDE = 480;

    inline QStringList fileSuffixes()
    {
        return QStringList() << "png" << "jpg" << "jpeg" << "bmp" << "gif";
    }

    inline bool isImageFile(const QString& path)
    {
        return fileSuffixes().contains(QFileInfo(path).suffix().toLower());
    }

    // The image as it is stored: scaled to fit MAX_SIDE, as a data URI. Empty for
    // an image that could not be read.
    inline QString encode(const QImage& source)
    {
        if (source.isNull())
            return QString();

        QImage image = source;
        if (image.width() > MAX_SIDE || image.height() > MAX_SIDE)
            image = image.scaled(MAX_SIDE, MAX_SIDE, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const bool alpha = image.hasAlphaChannel();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, alpha ? "PNG" : "JPG", alpha ? -1 : 85))
            return QString();

        return QString("data:image/%1;base64,%2").arg(alpha ? "png" : "jpeg", QString::fromLatin1(bytes.toBase64()));
    }

    inline QString fromFile(const QString& path)
    {
        QImage image;
        if (!image.load(path))
            return QString();
        return encode(image);
    }

    // Back to a picture, for the Inspector's preview.
    inline QImage decode(const QString& dataUri)
    {
        const int comma = dataUri.indexOf(',');
        if (!dataUri.startsWith("data:image/") || comma < 0)
            return QImage();

        QImage image;
        image.loadFromData(QByteArray::fromBase64(dataUri.mid(comma + 1).toLatin1()));
        return image;
    }

    // What the hover shows: the picture at its stored size.
    inline QString tooltipHtml(const QString& dataUri)
    {
        if (!dataUri.startsWith("data:image/"))
            return QString();
        return QString("<img src=\"%1\"/>").arg(dataUri);
    }
}
