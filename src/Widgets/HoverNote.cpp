#include "HoverNote.h"

#include "ReferenceImage.h"

#include "Commands/AbstractCommand.h"

#include <QtCore/QPointer>

#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

namespace
{
    const char* MARK_NAME = "hoverNoteMark";
}

void HoverNote::attach(QWidget* host, AbstractCommand* command, const QString& hint, const QPoint& markAt, bool withMark)
{
    if (host == nullptr || command == nullptr)
        return;

    apply(host, command, hint, markAt, withMark);

    // The host is the context: rebuilt keys and rows take their connections with them.
    QPointer<AbstractCommand> source(command);
    auto again = [host, source, hint, markAt, withMark]() {
        if (source != nullptr)
            apply(host, source.data(), hint, markAt, withMark);
    };
    QObject::connect(command, &AbstractCommand::noteChanged, host, again);
    QObject::connect(command, &AbstractCommand::referenceImageChanged, host, again);
}

void HoverNote::apply(QWidget* host, AbstractCommand* command, const QString& hint, const QPoint& markAt, bool withMark)
{
    const QString picture = command->getReferenceImage();
    const QString note = command->getNote();
    host->setToolTip(ReferenceImage::hoverWithHint(picture, note, hint));
    if (!withMark)
        return;

    QLabel* mark = host->findChild<QLabel*>(MARK_NAME, Qt::FindDirectChildrenOnly);
    const QString glyphs = ReferenceImage::markGlyphs(picture, note);
    if (glyphs.isEmpty())
    {
        if (mark != nullptr)
            mark->hide();
        return;
    }

    if (mark == nullptr)
    {
        mark = new QLabel(host);
        mark->setObjectName(MARK_NAME);
        mark->setAlignment(Qt::AlignCenter);
        mark->setAttribute(Qt::WA_TransparentForMouseEvents);
        mark->setStyleSheet("background-color: rgba(0, 0, 0, 110); color: white; border-radius: 3px; font-size: 10px; padding: 0px 2px;");
    }
    mark->setText(glyphs);
    mark->adjustSize();
    const int x = markAt.x() < 0 ? host->width() + markAt.x() - mark->width() : markAt.x();
    mark->move(x, markAt.y());
    mark->show();
    mark->raise();
}
