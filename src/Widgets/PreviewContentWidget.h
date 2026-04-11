#pragma once

#include "Shared.h"

#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT PreviewContentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewContentWidget(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void clearContent();
    bool isZoomed() const;

signals:
    void doubleClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QImage currentImage;
    bool zoomed = false;
    QPointF panOffset{0, 0};
    QPoint lastMousePos;
    bool dragging = false;
};
