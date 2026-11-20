#include "Rendering/CadView.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

void CadView::wheelEvent(QWheelEvent* event)
{
    if (!view_.IsNull()) {
        zoomBy(std::pow(1.15, event->angleDelta().y() / 120.0));
    }
    event->accept();
}

void CadView::mousePressEvent(QMouseEvent* event)
{
    previousMouse_ = event->position().toPoint();
}

void CadView::mouseMoveEvent(QMouseEvent* event)
{
    if (!view_.IsNull() && event->buttons().testFlag(Qt::MiddleButton)) {
        const QPoint next = event->position().toPoint();
        const QPoint delta = next - previousMouse_;
        panBy(delta.x(), -delta.y());
        previousMouse_ = next;
    }
}
