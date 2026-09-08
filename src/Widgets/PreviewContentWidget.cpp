#include "PreviewContentWidget.h"

#include "AudioLevelTrack.h"

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

void PreviewContentWidget::setAudioLevels(const QVector<double>& dbfs)
{
    if (this->audioLevels == dbfs)
        return;

    this->audioLevels = dbfs;
    update();
}

void PreviewContentWidget::clearAudioLevels()
{
    if (this->audioLevels.isEmpty())
        return;

    this->audioLevels.clear();
    update();
}

void PreviewContentWidget::setPlaceholder(const QString& text)
{
    if (this->placeholder == text)
        return;

    this->placeholder = text;
    update();
}

void PreviewContentWidget::paintAudioMeters(QPainter& painter, const QRectF& frame)
{
    if (this->audioLevels.isEmpty())
        return;

    // Sized from the frame rather than fixed, so the meters stay in proportion
    // whether the panel is a strip along the bottom of the screen or half of it.
    const int channels = this->audioLevels.size();
    const double barWidth = qBound(3.0, frame.width() * 0.012, 9.0);
    const double gap = qMax(1.0, barWidth * 0.35);
    const double inset = qMax(4.0, barWidth * 0.8);

    const double totalWidth = channels * barWidth + (channels - 1) * gap;
    const double height = qMax(24.0, frame.height() * 0.42);

    // Bottom right of the picture: the corner least likely to hold the thing the
    // operator is looking at, and where a meter is conventionally expected.
    const double left = frame.right() - inset - totalWidth;
    const double bottom = frame.bottom() - inset;
    const double top = bottom - height;

    if (left < frame.left() || top < frame.top())
        return;

    // A backing panel, because a green bar over bright picture is unreadable.
    QRectF backing(left - gap, top - gap, totalWidth + gap * 2, height + gap * 2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 110));
    painter.drawRoundedRect(backing, 2, 2);

    for (int channel = 0; channel < channels; channel++)
    {
        const double db = this->audioLevels.at(channel);

        // Same floor as AudioMeterWidget, so a level here and a level on the
        // server meters read the same.
        const double fraction = qBound(0.0, (db - AudioLevelTrack::DB_MIN) / (0.0 - AudioLevelTrack::DB_MIN), 1.0);

        const double x = left + channel * (barWidth + gap);

        // The unlit track first, so a silent channel still reads as a channel.
        painter.setBrush(QColor(255, 255, 255, 28));
        painter.drawRect(QRectF(x, top, barWidth, height));

        if (fraction <= 0.0)
            continue;

        const double litHeight = height * fraction;
        QRectF lit(x, bottom - litHeight, barWidth, litHeight);

        // Green until it is worth noticing, amber approaching the ceiling, red at
        // the top — the same thresholds the server meters use.
        QColor colour(0x00, 0xcc, 0x00);
        if (db > -3.0)
            colour = QColor(0xee, 0x00, 0x00);
        else if (db > -12.0)
            colour = QColor(0xdd, 0x99, 0x00);

        painter.setBrush(colour);
        painter.drawRect(lit);
    }
}

void PreviewContentWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), Qt::black);

    if (this->currentImage.isNull())
    {
        if (!this->placeholder.isEmpty())
        {
            QFont font = painter.font();
            font.setPixelSize(11);
            painter.setFont(font);
            painter.setPen(QColor(150, 150, 150));
            painter.drawText(rect().adjusted(12, 12, -12, -12),
                             Qt::AlignCenter | Qt::TextWordWrap, this->placeholder);
        }

        return;
    }

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

    // Clipped to the widget, because a zoomed picture reaches past its edges and
    // the meters belong to the panel, not to the picture.
    paintAudioMeters(painter, target.intersected(QRectF(rect())));

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
