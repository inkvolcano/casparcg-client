#pragma once

#include "Shared.h"

#include <QtCore/QPoint>
#include <QtCore/QString>

class AbstractCommand;
class QWidget;

// An item's reference picture and operator note on a Simple Mode key or a
// Shotbox row (build 309), as on rundown rows: hovering shows them, and a small
// mark in the corner says there is something to hover. Kept current when the
// note or picture changes; the connection goes with the host widget.
class WIDGETS_EXPORT HoverNote
{
    public:
        // hint is what the host's hover said before ("Drag to reorder"); it is
        // kept beneath. The mark sits at markAt, in the host's coordinates; a
        // negative x is measured from the host's right edge.
        static void attach(QWidget* host, AbstractCommand* command, const QString& hint, const QPoint& markAt, bool withMark = true);

    private:
        HoverNote() {}

        static void apply(QWidget* host, AbstractCommand* command, const QString& hint, const QPoint& markAt, bool withMark);
};
