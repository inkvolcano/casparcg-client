#pragma once

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtWidgets/QDialog>

namespace DialogPosition
{
    // Place a dialog near the cursor (above-left), clamped to the screen the cursor
    // is on so it can never open outside the visible area.
    inline void moveNearCursor(QDialog* dialog)
    {
        QPoint target(QCursor::pos().x() - dialog->width() + 40,
                      QCursor::pos().y() - dialog->height() - 10);

        QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
        if (screen == nullptr)
            screen = QGuiApplication::primaryScreen();
        if (screen != nullptr)
        {
            QRect available = screen->availableGeometry();
            target.setX(qBound(available.left(), target.x(), available.right() - dialog->width()));
            target.setY(qBound(available.top(), target.y(), available.bottom() - dialog->height()));
        }

        dialog->move(target);
    }
}
