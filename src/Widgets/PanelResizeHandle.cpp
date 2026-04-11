#include "PanelResizeHandle.h"
#include "PanelHelper.h"

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLayout>

PanelResizeHandle::PanelResizeHandle(QWidget* targetPanel, const QString& panelId, int defaultHeight, QWidget* parent)
    : QFrame(parent),
      targetPanel(targetPanel),
      panelId(panelId),
      defaultHeight(defaultHeight)
{
    setFixedHeight(6);
    setCursor(Qt::SizeVerCursor);
    setStyleSheet(
        "QFrame { background-color: rgba(45, 45, 45, 255); "
        "background-image: url(:/Graphics/Images/SplitterVertical.png); "
        "background-repeat: no-repeat; background-position: center; }");
}

int PanelResizeHandle::maxColumnHeight(QWidget* self)
{
    QWidget* column = self->parentWidget();
    if (!column || !column->layout())
        return 600;

    QLayout* lay = column->layout();

    int columnMax;

    // For grid layouts, use full compound height.
    // The grid naturally constrains via other cells' minimum heights.
    if (qobject_cast<QGridLayout*>(lay))
    {
        columnMax = column->height();
    }
    else
    {
        int siblingMin = 0;
        for (int i = 0; i < lay->count(); i++)
        {
            QLayoutItem* item = lay->itemAt(i);
            QWidget* w = item->widget();
            if (w && w != self)
            {
                int mh = w->minimumHeight();
                if (mh <= 0)
                    mh = w->minimumSizeHint().height();
                siblingMin += qMax(mh, 0);
            }
            else if (!w)
            {
                siblingMin += item->minimumSize().height();
            }
        }

        int sp = lay->spacing() * qMax(lay->count() - 1, 0);
        int top = 0, bot = 0;
        lay->getContentsMargins(nullptr, &top, nullptr, &bot);
        columnMax = column->height() - siblingMin - sp - top - bot;
    }

    // Also clamp to the window's available space — the panel can't grow
    // beyond the distance from its top edge to the window's bottom edge.
    QWidget* window = self->window();
    if (window != nullptr)
    {
        QPoint panelTopInWindow = self->mapTo(window, QPoint(0, 0));
        int windowMax = window->height() - panelTopInWindow.y() - 10;
        columnMax = qMin(columnMax, windowMax);
    }

    return columnMax;
}

void PanelResizeHandle::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->dragging = true;
        this->dragStartY = event->globalPosition().toPoint().y();
        this->dragStartHeight = this->targetPanel->height();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void PanelResizeHandle::mouseMoveEvent(QMouseEvent* event)
{
    if (this->dragging)
    {
        int delta = event->globalPosition().toPoint().y() - this->dragStartY;
        int maxH = qMax(maxColumnHeight(this->targetPanel), 80);
        int newH = qBound(80, this->dragStartHeight + delta, maxH);
        this->targetPanel->setFixedHeight(newH);
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void PanelResizeHandle::mouseReleaseEvent(QMouseEvent* event)
{
    if (this->dragging)
    {
        this->dragging = false;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, this->panelId + "PanelHeight",
                               QString::number(this->targetPanel->height())));
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void PanelResizeHandle::enterEvent(QEnterEvent* event)
{
    setCursor(Qt::SizeVerCursor);
    QFrame::enterEvent(event);
}

void PanelResizeHandle::leaveEvent(QEvent* event)
{
    unsetCursor();
    QFrame::leaveEvent(event);
}
