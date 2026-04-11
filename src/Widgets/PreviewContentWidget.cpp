#include "PreviewContentWidget.h"

#include <QtGui/QPainter>
#include <QtGui/QFont>

PreviewContentWidget::PreviewContentWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void PreviewContentWidget::setImage(const QImage& img)
{
    this->currentImage = img;
    update();
}

void PreviewContentWidget::clearContent()
{
    this->currentImage = QImage();
    this->zoomed = false;
    this->panOffset = {0, 0};
    update();
}

bool PreviewContentWidget::isZoomed() const
{
    return this->zoomed;
}

void PreviewContentWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), Qt::black);

    if (this->currentImage.isNull())
        return;

    QSizeF widgetSize = size();
    QSizeF imgSize = this->currentImage.size();

    // Compute the scale to fit image in widget.
    qreal fitScale = qMin(widgetSize.width() / imgSize.width(),
                          widgetSize.height() / imgSize.height());

    qreal scale = this->zoomed ? fitScale * 1.5 : fitScale;

    qreal scaledW = imgSize.width() * scale;
    qreal scaledH = imgSize.height() * scale;

    // Center the image, then apply pan offset when zoomed.
    qreal x = (widgetSize.width() - scaledW) / 2.0;
    qreal y = (widgetSize.height() - scaledH) / 2.0;

    if (this->zoomed)
    {
        x += this->panOffset.x();
        y += this->panOffset.y();

        // Clamp so the image can't be dragged completely off-screen.
        qreal overflowX = qMax(0.0, scaledW - widgetSize.width());
        qreal overflowY = qMax(0.0, scaledH - widgetSize.height());
        x = qBound(-overflowX / 2.0 - scaledW / 4.0, x, overflowX / 2.0 + scaledW / 4.0);
        y = qBound(-overflowY / 2.0 - scaledH / 4.0, y, overflowY / 2.0 + scaledH / 4.0);
    }

    QRectF target(x, y, scaledW, scaledH);
    painter.drawImage(target, this->currentImage);

    // Draw zoom badge when zoomed.
    if (this->zoomed)
    {
        // Tinted border.
        painter.setPen(QPen(QColor(80, 160, 255, 180), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));

        // Badge.
        QFont font = painter.font();
        font.setPixelSize(11);
        font.setBold(true);
        painter.setFont(font);

        QString badge = "150%";
        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(badge) + 8;
        int textH = fm.height() + 4;
        QRect badgeRect(width() - textW - 4, 4, textW, textH);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(badgeRect, 3, 3);

        painter.setPen(QColor(80, 160, 255));
        painter.drawText(badgeRect, Qt::AlignCenter, badge);
    }
}

void PreviewContentWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    this->zoomed = !this->zoomed;
    this->panOffset = {0, 0};

    if (this->zoomed)
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();

    update();
    emit doubleClicked();
}

void PreviewContentWidget::mousePressEvent(QMouseEvent* event)
{
    if (this->zoomed && event->button() == Qt::LeftButton)
    {
        this->dragging = true;
        this->lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PreviewContentWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (this->dragging)
    {
        QPoint delta = event->pos() - this->lastMousePos;
        this->panOffset += QPointF(delta);
        this->lastMousePos = event->pos();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void PreviewContentWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (this->dragging)
    {
        this->dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
