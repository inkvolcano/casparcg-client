#pragma once

#include "Shared.h"

#include <QtWidgets/QFrame>

class WIDGETS_EXPORT PanelResizeHandle : public QFrame
{
    Q_OBJECT

public:
    PanelResizeHandle(QWidget* targetPanel, const QString& panelId, int defaultHeight, QWidget* parent = nullptr);

    static int maxColumnHeight(QWidget* self);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QWidget* targetPanel = nullptr;
    QString panelId;
    int defaultHeight = 0;
    bool dragging = false;
    int dragStartY = 0;
    int dragStartHeight = 0;
};
