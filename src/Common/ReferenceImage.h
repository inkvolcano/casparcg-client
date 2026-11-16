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
#include <QtGui/QImageReader>
#include <QtCore/QSize>

namespace ReferenceImage
{
    static const int MAX_SIDE = 480;

    // Every hover shows its picture in the same frame: the user found pictures
    // shown at their own size "a bit big or small". 16:9, so a full-frame grab
    // fills it exactly; anything else is fitted inside it without distortion, and
    // a small picture is enlarged to fit like the rest.
    static const int HOVER_WIDTH = 384;
    static const int HOVER_HEIGHT = 216;

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

    // The size a picture of this size is shown at: fitted into the hover frame.
    inline QSize hoverSize(const QSize& picture)
    {
        if (picture.width() <= 0 || picture.height() <= 0)
            return QSize(HOVER_WIDTH, HOVER_HEIGHT);
        return picture.scaled(HOVER_WIDTH, HOVER_HEIGHT, Qt::KeepAspectRatio);
    }

    // The stored picture's size, read from its header only.
    inline QSize pictureSize(const QString& dataUri)
    {
        const int comma = dataUri.indexOf(',');
        if (!dataUri.startsWith("data:image/") || comma < 0)
            return QSize();

        QByteArray bytes = QByteArray::fromBase64(dataUri.mid(comma + 1).toLatin1());
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        return QImageReader(&buffer).size();
    }

    // What the hover shows: the picture, fitted into the hover frame.
    inline QString tooltipHtml(const QString& dataUri)
    {
        if (!dataUri.startsWith("data:image/"))
            return QString();

        const QSize size = hoverSize(pictureSize(dataUri));
        return QString("<img src=\"%1\" width=\"%2\" height=\"%3\"/>").arg(dataUri).arg(size.width()).arg(size.height());
    }

    // What hovering a row shows (build 299): its picture, then its operator note
    // beneath it - either alone when the other is not there, nothing when neither
    // is. The note is plain text: escaped, its line breaks kept.
    inline QString hoverHtml(const QString& dataUri, const QString& note)
    {
        const QString picture = tooltipHtml(dataUri);
        const QString text = note.trimmed();
        if (text.isEmpty())
            return picture;

        QString escaped = text.toHtmlEscaped();
        escaped.replace("\n", "<br/>");
        const QString paragraph = QString("<p style=\"margin-top: 6px; max-width: 384px;\"><b>Note:</b> %1</p>").arg(escaped);
        return picture.isEmpty() ? paragraph : picture + paragraph;
    }

    // The small mark on a key or a row that has something to hover (build 309):
    // a framed picture, a pencil, both, or nothing.
    inline QString markGlyphs(const QString& dataUri, const QString& note)
    {
        QString glyphs;
        if (dataUri.startsWith("data:image/"))
            glyphs += QString::fromUtf8("\xf0\x9f\x96\xbc");
        if (!note.trimmed().isEmpty())
            glyphs += QString::fromUtf8("\xe2\x9c\x8e");
        return glyphs;
    }

    // A hover that already had something to say ("Drag to reorder") keeps it,
    // beneath the picture and the note.
    inline QString hoverWithHint(const QString& dataUri, const QString& note, const QString& hint)
    {
        const QString hover = hoverHtml(dataUri, note);
        if (hover.isEmpty())
            return hint;
        if (hint.isEmpty())
            return hover;
        return hover + QString("<p style=\"margin-top: 6px; color: #9aa0a6;\">%1</p>").arg(hint.toHtmlEscaped());
    }
}

