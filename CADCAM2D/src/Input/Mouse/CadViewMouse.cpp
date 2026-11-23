#include "Rendering/CadView.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <utility>

void CadView::wheelEvent(QWheelEvent* event)
{
    if (!view_.IsNull()) {
        zoomBy(std::pow(1.15, event->angleDelta().y() / 120.0));
    }
    event->accept();
}

void CadView::mousePressEvent(QMouseEvent* event)
{
    if (view_.IsNull()) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        selectionStart_ = event->position().toPoint();
    } else if (event->button() == Qt::MiddleButton) {
        previousMouse_ = (event->position() * devicePixelRatioF()).toPoint();
    }
}

void CadView::mouseMoveEvent(QMouseEvent* event)
{
    if (view_.IsNull()) {
        return;
    }
    const QPoint logicalPosition = event->position().toPoint();
    if (event->buttons().testFlag(Qt::MiddleButton)) {
        const QPoint next = (event->position() * devicePixelRatioF()).toPoint();
        const QPoint delta = next - previousMouse_;
        panBy(delta.x(), -delta.y());
        previousMouse_ = next;
    } else if (selectionStart_ && event->buttons().testFlag(Qt::LeftButton)
        && (draggingSelection_ || (logicalPosition - *selectionStart_).manhattanLength()
            >= QApplication::startDragDistance())) {
        draggingSelection_ = true;
        showSelectionBox(*selectionStart_ * devicePixelRatioF(),
            (event->position() * devicePixelRatioF()).toPoint());
    }
}

void CadView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !selectionStart_) {
        return;
    }
    const QPoint start = *selectionStart_;
    selectionStart_.reset();
    const QPoint end = (event->position() * devicePixelRatioF()).toPoint();
    if (std::exchange(draggingSelection_, false)
        || (event->position().toPoint() - start).manhattanLength() >= QApplication::startDragDistance()) {
        selectBox(start * devicePixelRatioF(), end);
    } else {
        selectAt(end);
    }
}
