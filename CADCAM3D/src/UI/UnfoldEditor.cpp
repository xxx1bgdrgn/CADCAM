#include "UI/UnfoldEditor.h"

#include "Geometry/TubeUnfold.h"
#include "Rendering/ContourColor.h"
#include "Rendering/TubeView.h"
#include "Shape/OcctGeometry2D.h"

#include <BRepAdaptor_Curve.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <QActionGroup>
#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace cadcam3d {
namespace {

enum class Tool { Select, Point, Line, Arc, Circle, Rectangle, RoundedRectangle, Text, Ellipse, EllipticalArc, Polyline };
enum class CutLine { Lower, Upper };
using CanvasSelection = std::variant<std::monostate, cadcam::Id, CutLine>;

const QColor ContourYellow(cadcam::ContourRgb[0], cadcam::ContourRgb[1], cadcam::ContourRgb[2]);

QString cutLineName(CutLine cut)
{
    return cut == CutLine::Upper ? QStringLiteral("上截断线") : QStringLiteral("下截断线");
}

struct ToolInfo { const char* name; const char* hint; int clicks; };
constexpr std::array<ToolInfo, 11> tools{{
    {"选择", "点击图形选中，Delete 删除。", 0},
    {"点", "点击插入位置。", 1},
    {"直线", "依次指定起点、终点。", 2},
    {"圆弧", "依次指定起点、弧上一点、终点。", 3},
    {"圆", "先点圆心，再点圆周。", 2},
    {"矩形", "点击两个对角点。", 2},
    {"圆角矩形", "点击两个对角点，再输入圆角半径。", 2},
    {"文字", "点击插入位置，再输入文字和字高。", 1},
    {"椭圆", "点击外接矩形的两个对角点。", 2},
    {"椭圆弧", "点击外接矩形的两个对角点，再指定起、止方向。", 4},
    {"多段线", "逐点绘制。回车或右键结束，点击起点闭合。", 0},
}};

const ToolInfo& info(Tool tool)
{
    return tools[static_cast<std::size_t>(tool)];
}

constexpr double Pi = 3.14159265358979323846;
constexpr int RulerSize = 25;

// Sketch geometry still uses (axial, circumferential); the editor shows (circumferential, axial).
QPointF scenePoint(cadcam::Point point) { return {point.y, -point.x}; }

double distance(cadcam::Point a, cadcam::Point b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

cadcam::Rectangle rectangle(cadcam::Point a, cadcam::Point b)
{
    return {{std::min(a.x, b.x), std::min(a.y, b.y)}, std::abs(a.x - b.x), std::abs(a.y - b.y)};
}

cadcam::Ellipse ellipse(const cadcam::Rectangle& box)
{
    return {{box.lowerLeft.x + box.width / 2, box.lowerLeft.y + box.height / 2},
        std::max(box.width, box.height) / 2, std::min(box.width, box.height) / 2,
        box.width >= box.height ? 0.0 : 90.0};
}

double ellipseParameter(const cadcam::Ellipse& oval, cadcam::Point point)
{
    const double x = point.x - oval.center.x, y = point.y - oval.center.y;
    const double p = oval.rotationDegrees == 0
        ? std::atan2(y / oval.minorRadius, x / oval.majorRadius)
        : std::atan2(-x / oval.minorRadius, y / oval.majorRadius);
    return p < 0 ? p + 2 * Pi : p;
}

cadcam::Polyline polyline(const std::vector<cadcam::Point>& points, bool closed)
{
    cadcam::Polyline line{{}, closed};
    for (std::size_t i = 1; i < points.size(); ++i) {
        line.segments.emplace_back(cadcam::Line{points[i - 1], points[i]});
    }
    return line;
}

QPainterPath primitivePath(const cadcam::Primitive& primitive)
{
    QPainterPath path;
    if (const auto* point = std::get_if<cadcam::Point>(&primitive)) {
        const auto center = scenePoint(*point);
        path.addEllipse(center, 0.5, 0.5);
    } else if (const auto* label = std::get_if<cadcam::Text>(&primitive)) {
        path = QTransform(0, -1, 1, 0, 0, 0).map(unfoldedTextPath(*label));
    } else {
        const auto shape = cadcam::makeOcctShape(primitive);
        for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
            const int samples = curve.GetType() == GeomAbs_Line ? 1 : 64;
            for (int i = 0; i <= samples; ++i) {
                const auto at = curve.Value(curve.FirstParameter() +
                    (curve.LastParameter() - curve.FirstParameter()) * i / samples);
                const auto display = scenePoint({at.X(), at.Y()});
                if (i == 0) path.moveTo(display);
                else path.lineTo(display);
            }
        }
    }
    return path;
}

QPainterPath shapePath(const SketchShape& shape)
{
    if (const auto* primitive = std::get_if<cadcam::Primitive>(&shape.content)) return primitivePath(*primitive);
    QPainterPath path;
    for (const auto& member : std::get<std::vector<SketchShape>>(shape.content)) path.addPath(shapePath(member));
    return path;
}

class SketchEdit final : public QUndoCommand
{
public:
    SketchEdit(std::function<void(bool)> change, const QString& description) : QUndoCommand(description),
        change_(std::move(change)) {}
    void redo() override { change_(true); }
    void undo() override { change_(false); }

private:
    std::function<void(bool)> change_;
};

}

class UnfoldCanvas final : public QGraphicsView
{
public:
    UnfoldCanvas(const TubeUnfold& unfold, std::vector<SketchShape> sketches, QLabel& hint, QLabel& coordinates,
        QAction& deleteAction, QWidget* parent)
        : QGraphicsView(parent),
          surface_(-unfold.firstFaceHalfWidth(), -unfold.length(), unfold.perimeter(), unfold.length()),
          guides_(unfold.guides()), sketches_(std::move(sketches)), hint_(hint), coordinates_(coordinates),
          deleteAction_(deleteAction)
    {
        setScene(new QGraphicsScene(this));
        setRenderHint(QPainter::Antialiasing);
        setTransformationAnchor(NoAnchor);
        setResizeAnchor(AnchorViewCenter);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setViewportUpdateMode(FullViewportUpdate);
        setMouseTracking(true);
        setMinimumSize(520, 330);
        setSceneRect(viewArea().adjusted(-surface_.width(), -surface_.height(), surface_.width(), surface_.height()));
        for (const auto& shape : sketches_) nextId_ = std::max(nextId_, shape.id + 1);
        setTool(Tool::Select);
    }

    const std::vector<SketchShape>& sketches() const { return sketches_; }
    QUndoStack& history() { return history_; }
    bool hasDraft() const { return !points_.empty(); }

    void fit()
    {
        fitInView(viewArea(), Qt::KeepAspectRatio);
        centerOn(viewArea().center());
    }

    void setTool(Tool tool)
    {
        tool_ = tool;
        points_.clear();
        setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        select({});
    }

    void deleteSelected()
    {
        const auto* id = std::get_if<cadcam::Id>(&selected_);
        if (!id) return;
        const auto found = std::find_if(sketches_.begin(), sketches_.end(),
            [id](const SketchShape& shape) { return shape.id == *id; });
        if (found == sketches_.end()) return;
        const auto index = std::distance(sketches_.begin(), found);
        history_.push(new SketchEdit([this, index, shape = *found](bool redo) {
            if (redo) sketches_.erase(sketches_.begin() + index);
            else sketches_.insert(sketches_.begin() + index, shape);
            select({});
        }, QStringLiteral("删除图形")));
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QGraphicsView::showEvent(event);
        fit();
    }

    void drawBackground(QPainter* painter, const QRectF& exposed) override
    {
        painter->fillRect(exposed, QColor(23, 31, 39));
        painter->fillRect(surface_, QColor(32, 46, 57));
        const auto view = exposed.intersected(surface_);
        if (view.isEmpty()) return;

        const double unitsPerPixel = 1 / transform().m11();
        const double spacing = std::pow(10., std::ceil(std::log10(18 * unitsPerPixel)));
        QPen grid(QColor(53, 70, 79));
        grid.setCosmetic(true);
        painter->setPen(grid);
        for (double u = std::ceil(view.left() / spacing) * spacing; u <= view.right(); u += spacing)
            painter->drawLine(QPointF(u, view.top()), QPointF(u, view.bottom()));
        for (double v = std::ceil(view.top() / spacing) * spacing; v <= view.bottom(); v += spacing) {
            if (v > surface_.top() && v < surface_.bottom())
                painter->drawLine(QPointF(view.left(), v), QPointF(view.right(), v));
        }

        QPen panel(QColor(191, 209, 212), 2);
        panel.setCosmetic(true);
        painter->setPen(panel);
        for (const double at : guides_)
            painter->drawLine(QPointF(at, surface_.top()), QPointF(at, surface_.bottom()));

        QPen outline(QColor(127, 170, 175));
        outline.setCosmetic(true);
        painter->setPen(outline);
        painter->drawLine(surface_.topLeft(), surface_.bottomLeft());
        painter->drawLine(surface_.topRight(), surface_.bottomRight());
    }

    void drawForeground(QPainter* painter, const QRectF&) override
    {
        const auto view = viewportTransform();
        const auto origin = view.map(QPointF());
        const auto top = view.map(QPointF(0, surface_.top()));
        const auto lower = screenCutLine(CutLine::Lower);
        const auto end = lower.p2();
        const auto visible = viewport()->rect();
        painter->save();
        painter->resetTransform();
        painter->setPen(QColor(86, 181, 137));
        if (lower.x1() > RulerSize) painter->drawLine(QPointF(RulerSize, origin.y()), lower.p1());
        if (lower.x2() < visible.right()) painter->drawLine(lower.p2(), QPointF(visible.right(), origin.y()));
        painter->drawLine(QPointF(origin.x(), RulerSize), QPointF(origin.x(), visible.bottom()));
        painter->drawText(QPointF(visible.right() - 18, origin.y() - 6), QStringLiteral("X"));
        painter->drawText(QPointF(origin.x() + 6, RulerSize + 14), QStringLiteral("Y"));
        painter->drawText(origin + QPointF(6, 17), QStringLiteral("(0, 0)"));
        painter->setPen(QColor(165, 210, 213));
        painter->drawText(top + QPointF(3, -5), QStringLiteral("Y 轴向 %1 mm").arg(surface_.height()));
        const auto circumference = QStringLiteral("周长 %1 mm").arg(surface_.width(), 0, 'f', 2);
        painter->drawText(end + QPointF(-painter->fontMetrics().horizontalAdvance(circumference), 17), circumference);
        for (const auto cut : {CutLine::Lower, CutLine::Upper}) {
            const auto line = screenCutLine(cut);
            const auto* current = std::get_if<CutLine>(&selected_);
            const bool selected = current && *current == cut;
            painter->setPen(QPen(ContourYellow, selected ? 2.5 : 1.5, selected ? Qt::DashLine : Qt::SolidLine));
            painter->drawLine(line);
            const auto label = cutLineName(cut);
            painter->setPen(ContourYellow);
            painter->drawText(line.p2() - QPointF(painter->fontMetrics().horizontalAdvance(label) + 5, 6), label);
        }
        drawRulers(*painter, visible, origin, view.m11());
        painter->restore();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const double factor = std::pow(1.0015, event->angleDelta().y());
        const double zoom = factor * transform().m11();
        if (zoom > 0.03 && zoom < 500) {
            const auto cursor = event->position().toPoint();
            const auto anchor = mapToScene(cursor);
            scale(factor, factor);
            const auto shift = mapFromScene(anchor) - cursor;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + shift.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() + shift.y());
        }
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->position().x() < RulerSize || event->position().y() < RulerSize) {
            event->accept();
            return;
        }
        setFocus();
        if (event->button() == Qt::MiddleButton) {
            panAnchor_ = event->position().toPoint();
            setCursor(Qt::ClosedHandCursor);
        } else if (event->button() == Qt::RightButton) {
            if (tool_ == Tool::Polyline && points_.size() >= 2) finishPolyline(false);
            else cancelDraft();
        } else if (event->button() == Qt::LeftButton) {
            const auto at = event->position().toPoint();
            if (tool_ == Tool::Select) selectNear(at);
            else placePoint(at);
        }
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const auto at = event->position().toPoint();
        if (panAnchor_) {
            const auto delta = at - *panAnchor_;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            panAnchor_ = at;
        }
        const auto point = modelPoint(at);
        coordinates_.setText(QStringLiteral("X 周向 %1 mm  ·  Y 轴向 %2 mm")
            .arg(point.y, 0, 'f', 2).arg(point.x, 0, 'f', 2));
        if (!panAnchor_ && !points_.empty()) updatePreview(point);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton) {
            panAnchor_.reset();
            setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        }
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            tool_ == Tool::Polyline && points_.size() >= 2) {
            finishPolyline(false);
            return;
        }
        if (event->key() == Qt::Key_Escape && !points_.empty()) {
            cancelDraft();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

private:
    void drawRulers(QPainter& painter, const QRect& visible, QPointF origin, double pixelsPerMm) const
    {
        const QColor background(30, 42, 50);
        const QColor marks(135, 155, 167);
        const QColor labels(205, 220, 225);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(0, 0, visible.width(), RulerSize, background);
        painter.fillRect(0, 0, RulerSize, visible.height(), background);
        painter.setPen(marks);
        painter.drawLine(RulerSize - 1, 0, RulerSize - 1, visible.bottom());
        painter.drawLine(0, RulerSize - 1, visible.right(), RulerSize - 1);

        const double desired = 52 / pixelsPerMm;
        const double decade = std::pow(10., std::floor(std::log10(desired)));
        const double ratio = desired / decade;
        const double majorStep = decade * (ratio <= 2 ? 2 : ratio <= 5 ? 5 : 10);
        const double minorStep = majorStep / 10;
        const double spacing = minorStep * pixelsPerMm;
        const auto fontMetrics = painter.fontMetrics();
        for (bool horizontal : {true, false}) {
            const double zero = horizontal ? origin.x() : origin.y();
            const int extent = horizontal ? visible.width() : visible.height();
            const auto first = static_cast<qint64>(std::ceil((RulerSize - zero) / spacing));
            const auto last = static_cast<qint64>(std::floor((extent - 1 - zero) / spacing));
            painter.save();
            painter.setClipRect(horizontal ? QRect(RulerSize, 0, extent - RulerSize, RulerSize)
                : QRect(0, RulerSize, RulerSize, extent - RulerSize));
            for (auto tick = first; tick <= last; ++tick) {
                const double at = zero + tick * spacing;
                const bool major = tick % 10 == 0;
                const int tip = RulerSize - (major ? 10 : tick % 5 == 0 ? 7 : 4);
                painter.setPen(marks);
                if (horizontal) painter.drawLine(QPointF(at, tip), QPointF(at, RulerSize - 1));
                else painter.drawLine(QPointF(tip, at), QPointF(RulerSize - 1, at));
                if (!major) continue;
                const auto value = QString::number((horizontal ? tick : -tick) * minorStep, 'g', 8);
                const double labelWidth = fontMetrics.horizontalAdvance(value);
                if (horizontal ? at + 3 + labelWidth > extent : at - 3 - labelWidth < RulerSize) continue;
                painter.setPen(labels);
                if (horizontal) {
                    painter.drawText(QPointF(at + 3, 12), value);
                } else {
                    painter.save();
                    painter.translate(12, at - 3);
                    painter.rotate(-90);
                    painter.drawText(QPointF(), value);
                    painter.restore();
                }
            }
            painter.restore();
        }
        painter.setPen(labels);
        painter.drawText(QRect(0, 0, RulerSize - 1, RulerSize - 1), Qt::AlignCenter, QStringLiteral("mm"));
    }

    QRectF viewArea() const
    {
        const double xMargin = std::max(12.0, surface_.width() * 0.12);
        const double yMargin = std::max(12.0, surface_.height() * 0.12);
        return surface_.adjusted(-xMargin, -yMargin, xMargin, yMargin);
    }

    cadcam::Point modelPoint(QPoint at) const
    {
        const auto coordinates = mapToScene(at);
        return {-coordinates.y(), coordinates.x()};
    }

    QLineF screenCutLine(CutLine cut) const
    {
        const double y = cut == CutLine::Upper ? surface_.top() : surface_.bottom();
        return viewportTransform().map(QLineF(surface_.left(), y, surface_.right(), y));
    }

    void selectNear(QPoint at)
    {
        for (auto* item : items(QRect(at - QPoint(4, 4), QSize(9, 9)))) {
            if (item->data(0).isValid()) {
                select(static_cast<cadcam::Id>(item->data(0).toULongLong()));
                return;
            }
        }
        double nearestDistance = 6;
        CanvasSelection selection;
        for (const auto cut : {CutLine::Lower, CutLine::Upper}) {
            const auto line = screenCutLine(cut);
            const double distance = std::abs(at.y() - line.y1());
            if (at.x() >= line.x1() - 5 && at.x() <= line.x2() + 5 && distance < nearestDistance) {
                nearestDistance = distance;
                selection = cut;
            }
        }
        select(selection);
    }

    void describeSelection()
    {
        if (const auto* id = std::get_if<cadcam::Id>(&selected_)) {
            hint_.setText(QStringLiteral("已选择图形 #%1").arg(*id));
        } else if (const auto* cut = std::get_if<CutLine>(&selected_)) {
            hint_.setText(QStringLiteral("已选择%1 · 管材端部").arg(cutLineName(*cut)));
        } else {
            hint_.setText(QString::fromUtf8(info(tool_).hint));
        }
    }

    void select(CanvasSelection selection)
    {
        selected_ = selection;
        deleteAction_.setEnabled(std::holds_alternative<cadcam::Id>(selected_));
        describeSelection();
        updateScene();
    }

    void updateScene()
    {
        scene()->clear();
        auto* clip = scene()->addRect(surface_, QPen(Qt::NoPen));
        clip->setFlag(QGraphicsItem::ItemClipsChildrenToShape);
        for (const auto& shape : sketches_) {
            const auto path = shapePath(shape);
            const auto* id = std::get_if<cadcam::Id>(&selected_);
            const bool selected = id && *id == shape.id;
            QPen pen(ContourYellow, 1.7, selected ? Qt::DashLine : Qt::SolidLine);
            pen.setCosmetic(true);
            for (const int period : {-1, 0, 1}) {
                const auto copy = path.translated(period * surface_.width(), 0);
                auto* item = new QGraphicsPathItem(copy, clip);
                item->setPen(pen);
                item->setData(0, QVariant::fromValue<qulonglong>(shape.id));
            }
        }
        QPen draft(ContourYellow, 1.5, Qt::DashLine);
        draft.setCosmetic(true);
        preview_ = new QGraphicsPathItem(clip);
        preview_->setPen(draft);
        viewport()->update();
    }

    void cancelDraft()
    {
        points_.clear();
        preview_->setPath({});
        describeSelection();
    }

    void updatePreview(cadcam::Point point)
    {
        QPainterPath path(scenePoint(points_.front()));
        const auto area = QRectF(scenePoint(points_.front()), scenePoint(point)).normalized();
        if (tool_ == Tool::Rectangle || tool_ == Tool::RoundedRectangle) {
            path.addRect(area);
        } else if (tool_ == Tool::Circle) {
            const auto radius = distance(points_.front(), point);
            path.addEllipse(scenePoint(points_.front()), radius, radius);
        } else if (tool_ == Tool::Ellipse || tool_ == Tool::EllipticalArc) {
            if (points_.size() < 2) path.addEllipse(area);
            else path = primitivePath(cadcam::Primitive{ellipse(rectangle(points_[0], points_[1]))});
        } else if (tool_ == Tool::Arc && points_.size() == 2) {
            try {
                path = primitivePath(cadcam::Primitive{cadcam::CircularArc{points_[0], points_[1], point}});
            } catch (const Standard_Failure&) {
                path.lineTo(scenePoint(points_[1]));
                path.lineTo(scenePoint(point));
            }
        } else {
            for (std::size_t i = 1; i < points_.size(); ++i) path.lineTo(scenePoint(points_[i]));
            path.lineTo(scenePoint(point));
        }
        preview_->setPath(path);
    }

    void placePoint(QPoint at)
    {
        const auto point = modelPoint(at);
        if (!viewArea().contains(scenePoint(point))) return;
        if (point.x < 0 || point.x > surface_.height() ||
            (points_.empty() && (point.y < surface_.left() || point.y > surface_.right()))) {
            hint_.setText(QStringLiteral("起点须在管材展开范围内，轴向须在管长范围内。"));
            return;
        }
        if (!points_.empty() && distance(points_.back(), point) < 1e-5) return;
        if (tool_ == Tool::EllipticalArc && points_.size() == 1) {
            const auto box = rectangle(points_.front(), point);
            if (std::min(box.width, box.height) < 1e-3) {
                hint_.setText(QStringLiteral("椭圆弧需要非零宽度和高度，请重新选择对角点。"));
                return;
            }
        }
        if (tool_ == Tool::Polyline && points_.size() >= 3 &&
            distance(points_.front(), point) <= 8 / transform().m11()) {
            finishPolyline(true);
            return;
        }
        points_.push_back(point);
        const auto clicks = info(tool_).clicks;
        if (clicks != 0 && static_cast<int>(points_.size()) == clicks) {
            finishPrimitive();
        } else {
            updatePreview(point);
        }
    }

    void add(cadcam::Primitive primitive)
    {
        try {
            if (primitivePath(primitive).isEmpty()) throw std::invalid_argument("图形尺寸无效。");
        } catch (const Standard_Failure&) {
            hint_.setText(QStringLiteral("无法构造图形，请检查几何尺寸后再试。"));
            return;
        } catch (const std::exception& error) {
            hint_.setText(QString::fromUtf8(error.what()));
            return;
        }
        history_.push(new SketchEdit([this, shape = SketchShape{nextId_++, std::move(primitive), {}}](bool redo) {
            if (redo) sketches_.push_back(shape);
            else sketches_.pop_back();
            select({});
        }, QStringLiteral("添加图形")));
    }

    void finishPolyline(bool closed)
    {
        auto line = polyline(points_, closed);
        cancelDraft();
        add(std::move(line));
    }

    void finishPrimitive()
    {
        auto points = std::move(points_);
        cancelDraft();
        const auto a = points[0];
        if (tool_ == Tool::Point) { add(a); return; }
        if (tool_ == Tool::Text) {
            bool ok = false;
            const auto value = QInputDialog::getText(this, QStringLiteral("文字"), QStringLiteral("内容"),
                QLineEdit::Normal, {}, &ok);
            if (!ok || value.trimmed().isEmpty()) return;
            const auto height = QInputDialog::getDouble(this, QStringLiteral("字高"), QStringLiteral("毫米"),
                5.0, 0.1, 1000.0, 2, &ok);
            if (ok) add(cadcam::Text{a, value.toUtf8().toStdString(), font().family().toStdString(), height, 90});
            return;
        }
        const auto b = points[1];
        if (tool_ == Tool::Line) { add(cadcam::Line{a, b}); return; }
        if (tool_ == Tool::Arc) { add(cadcam::CircularArc{a, b, points[2]}); return; }
        if (tool_ == Tool::Circle) { add(cadcam::Circle{a, distance(a, b)}); return; }

        const auto box = rectangle(a, b);
        const double shortSide = std::min(box.width, box.height);
        if (shortSide < 1e-3) {
            hint_.setText(QStringLiteral("图形需要非零宽度和高度。"));
            return;
        }
        if (tool_ == Tool::Rectangle) { add(box); return; }
        if (tool_ == Tool::RoundedRectangle) {
            bool ok = false;
            const auto radius = QInputDialog::getDouble(this, QStringLiteral("圆角半径"), QStringLiteral("毫米"),
                std::min(3., shortSide / 4), 0.0001, shortSide / 2 - 0.0001, 4, &ok);
            if (ok) add(cadcam::RoundedRectangle{box, radius});
            return;
        }
        const auto oval = ellipse(box);
        if (tool_ == Tool::Ellipse) { add(oval); return; }
        const double start = ellipseParameter(oval, points[2]);
        double end = ellipseParameter(oval, points[3]);
        if (end <= start) end += 2 * Pi;
        add(cadcam::EllipticalArc{oval, start, end});
    }

    const QRectF surface_;
    std::vector<double> guides_;
    std::vector<SketchShape> sketches_;
    QLabel& hint_;
    QLabel& coordinates_;
    QAction& deleteAction_;
    QUndoStack history_;
    CanvasSelection selected_;
    cadcam::Id nextId_ = 1;
    Tool tool_ = Tool::Select;
    std::vector<cadcam::Point> points_;
    std::optional<QPoint> panAnchor_;
    QGraphicsPathItem* preview_ = nullptr;
};

UnfoldEditor::UnfoldEditor(const QString& title, const TubeUnfold& unfold, const TubePart& part,
    QWidget* parent)
    : QDialog(parent), unfold_(unfold)
{
    setWindowTitle(QStringLiteral("二维展开编辑  ·  %1").arg(title));
    setWindowFlag(Qt::WindowMaximizeButtonHint);
    resize(1120, 720);
    auto* layout = new QVBoxLayout(this);
    auto* commandBar = new QToolBar(this);
    auto* drawingBar = new QToolBar(this);
    for (auto* bar : {commandBar, drawingBar}) {
        bar->setStyleSheet(QStringLiteral("QToolBar::separator { background: palette(mid); width: 1px; margin: 3px 7px; }"));
    }
    auto* hint = new QLabel(this);
    auto* coordinates = new QLabel(this);
    auto* deleteAction = new QAction(QStringLiteral("删除"), this);
    auto* footer = new QHBoxLayout();
    auto* workspace = new QSplitter(Qt::Horizontal, this);
    workspace->setChildrenCollapsible(false);
    workspace->setHandleWidth(5);
    auto* previewPanel = new QWidget(workspace);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 4, 0);
    previewLayout->addWidget(new QLabel(QStringLiteral("三维预览"), previewPanel));
    auto* partLabel = new QLabel(title, previewPanel);
    partLabel->setWordWrap(true);
    previewLayout->addWidget(partLabel);
    auto* preview = new TubeView(previewPanel);
    preview->setMinimumSize(204, 220);
    previewLayout->addWidget(preview, 1);
    canvas_ = new UnfoldCanvas(unfold, part.unfoldedShapes, *hint, *coordinates, *deleteAction, workspace);
    workspace->addWidget(previewPanel);
    workspace->addWidget(canvas_);
    workspace->setStretchFactor(1, 1);
    workspace->setSizes({250, 850});
    QTimer::singleShot(0, preview, [preview, &part, partLabel] {
        try {
            preview->draw(part);
        } catch (const Standard_Failure&) {
            partLabel->setText(QStringLiteral("三维预览无法加载"));
        }
    });
    layout->addWidget(commandBar);
    layout->addWidget(drawingBar);
    layout->addWidget(workspace, 1);
    layout->addLayout(footer);
    footer->addWidget(hint, 1);
    footer->addWidget(coordinates);

    commandBar->addAction(QStringLiteral("适配视图"), canvas_, [this] { canvas_->fit(); });
    commandBar->addSeparator();
    auto* undo = canvas_->history().createUndoAction(this, QStringLiteral("撤销"));
    auto* redo = canvas_->history().createRedoAction(this, QStringLiteral("重做"));
    undo->setShortcut(QKeySequence::Undo);
    redo->setShortcut(QKeySequence::Redo);
    commandBar->addAction(undo);
    commandBar->addAction(redo);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, canvas_, &UnfoldCanvas::deleteSelected);
    commandBar->addAction(deleteAction);
    commandBar->addSeparator();
    commandBar->addAction(QStringLiteral("确定"), this, &UnfoldEditor::accept);
    commandBar->addAction(QStringLiteral("取消"), this, &UnfoldEditor::reject);

    auto* modes = new QActionGroup(this);
    for (std::size_t i = 0; i < tools.size(); ++i) {
        const auto tool = static_cast<Tool>(i);
        if (tool == Tool::Point || tool == Tool::Rectangle || tool == Tool::Text) drawingBar->addSeparator();
        auto* action = drawingBar->addAction(QString::fromUtf8(info(tool).name));
        action->setToolTip(QString::fromUtf8(info(tool).hint));
        action->setCheckable(true);
        modes->addAction(action);
        action->setChecked(tool == Tool::Select);
        connect(action, &QAction::triggered, canvas_, [this, tool] { canvas_->setTool(tool); });
    }
}

void UnfoldEditor::accept()
{
    if (canvas_->hasDraft()) {
        QMessageBox::information(this, QStringLiteral("绘图尚未完成"),
            QStringLiteral("请先画完当前图形，或按 Esc 取消这一笔。"));
        return;
    }
    try {
        UnfoldEdits edits;
        edits.sketches = canvas_->sketches();
        edits.mapped.reserve(edits.sketches.size());
        for (const auto& shape : edits.sketches) edits.mapped.push_back(unfold_.mapShape(shape));
        edits_ = std::move(edits);
        QDialog::accept();
    } catch (const Standard_Failure& error) {
        QMessageBox::warning(this, QStringLiteral("无法回写图形"), QString::fromUtf8(error.GetMessageString()));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, QStringLiteral("无法回写图形"), QString::fromUtf8(error.what()));
    }
}

UnfoldEdits UnfoldEditor::takeEdits()
{
    return std::move(edits_);
}

}
