#pragma once

#include "Shared.h"

#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT PreviewContentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewContentWidget(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void clearContent();
    bool isZoomed() const;

    // Audio meters drawn over the picture, one bar per channel, in dBFS. They sit
    // inside the drawn frame rather than beside it so they scale with the picture
    // and cost the panel no height of its own.
    void setAudioLevels(const QVector<double>& dbfs);
    void clearAudioLevels();

    // Shown centred when there is nothing to draw, so an empty box says why it is
    // empty instead of just being black.
    void setPlaceholder(const QString& text);

signals:
    void doubleClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void paintAudioMeters(QPainter& painter, const QRectF& frame);

    QImage currentImage;
    QVector<double> audioLevels;
    QString placeholder;
    bool zoomed = false;
    QPointF panOffset{0, 0};
    QPoint lastMousePos;
    bool dragging = false;
};
