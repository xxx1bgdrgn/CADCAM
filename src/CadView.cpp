#include "CadView.h"

#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <WNT_Window.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Trsf.hxx>

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace {

using namespace cadcam;

constexpr double Pi = 3.14159265358979323846;

gp_Pnt point(Point xy)
{
    return {xy.x, xy.y, 0};
}

TopoDS_Edge edge(const Line& line)
{
    return BRepBuilderAPI_MakeEdge(point(line.start), point(line.end)).Edge();
}

TopoDS_Edge edge(const CircularArc& arc)
{
    const auto curve = GC_MakeArcOfCircle(point(arc.start), point(arc.through), point(arc.end)).Value();
    return BRepBuilderAPI_MakeEdge(curve).Edge();
}

gp_Elips ellipseCurve(const Ellipse& ellipse)
{
    const double angle = ellipse.rotationDegrees * Pi / 180.0;
    const gp_Ax2 axes(point(ellipse.center), gp::DZ(), gp_Dir(std::cos(angle), std::sin(angle), 0));
    return {axes, ellipse.majorRadius, ellipse.minorRadius};
}

TopoDS_Shape makeShape(const Point& marker)
{
    return BRepBuilderAPI_MakeVertex(point(marker)).Vertex();
}

TopoDS_Shape makeShape(const Line& line)
{
    return edge(line);
}

TopoDS_Shape makeShape(const CircularArc& arc)
{
    return edge(arc);
}

TopoDS_Shape makeShape(const Circle& circle)
{
    return BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(point(circle.center), gp::DZ()), circle.radius)).Edge();
}

TopoDS_Shape makeShape(const Rectangle& rectangle)
{
    const auto [x, y] = rectangle.lowerLeft;
    const double right = x + rectangle.width;
    const double top = y + rectangle.height;
    return BRepBuilderAPI_MakePolygon(point({x, y}), point({right, y}),
        point({right, top}), point({x, top}), true).Wire();
}

TopoDS_Shape makeShape(const RoundedRectangle& rounded)
{
    const Rectangle& rectangle = rounded.boundary;
    const auto [x, y] = rectangle.lowerLeft;
    const double right = x + rectangle.width;
    const double top = y + rectangle.height;
    const double r = rounded.radius;
    if (r <= 0 || r >= std::min(rectangle.width, rectangle.height) / 2.0) {
        return makeShape(rectangle);
    }
    const double inset = r * (1.0 - std::sqrt(0.5));

    BRepBuilderAPI_MakeWire wire;
    wire.Add(edge(Line{{x + r, y}, {right - r, y}}));
    wire.Add(edge(CircularArc{{right - r, y}, {right - inset, y + inset}, {right, y + r}}));
    wire.Add(edge(Line{{right, y + r}, {right, top - r}}));
    wire.Add(edge(CircularArc{{right, top - r}, {right - inset, top - inset}, {right - r, top}}));
    wire.Add(edge(Line{{right - r, top}, {x + r, top}}));
    wire.Add(edge(CircularArc{{x + r, top}, {x + inset, top - inset}, {x, top - r}}));
    wire.Add(edge(Line{{x, top - r}, {x, y + r}}));
    wire.Add(edge(CircularArc{{x, y + r}, {x + inset, y + inset}, {x + r, y}}));
    return wire.Wire();
}

TopoDS_Shape makeShape(const Ellipse& ellipse)
{
    return BRepBuilderAPI_MakeEdge(ellipseCurve(ellipse)).Edge();
}

TopoDS_Shape makeShape(const EllipticalArc& arc)
{
    return BRepBuilderAPI_MakeEdge(ellipseCurve(arc.ellipse), arc.startParameter, arc.endParameter).Edge();
}

TopoDS_Shape makeShape(const Polyline& polyline)
{
    BRepBuilderAPI_MakeWire wire;
    for (const auto& segment : polyline.segments) {
        wire.Add(std::visit([](const auto& value) { return edge(value); }, segment));
    }
    if (polyline.closed && !polyline.segments.empty()) {
        const Point start = std::visit([](const auto& value) { return value.start; }, polyline.segments.front());
        const Point end = std::visit([](const auto& value) { return value.end; }, polyline.segments.back());
        if (std::hypot(start.x - end.x, start.y - end.y) > 1e-7) {
            wire.Add(edge(Line{end, start}));
        }
    }
    return wire.IsDone() ? TopoDS_Shape(wire.Wire()) : TopoDS_Shape{};
}

gp_Trsf placement(const RenderItem& item)
{
    gp_Trsf transform;
    transform.SetRotation(gp::OZ(), item.rotationDegrees * Pi / 180.0);
    transform.SetTranslationPart(gp_Vec(item.offset.x, item.offset.y, 0));
    return transform;
}

}

CadView::CadView(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMinimumSize(440, 340);
}

CadView::~CadView()
{
    if (!view_.IsNull()) {
        view_->Remove();
    }
}

void CadView::initialize()
{
    if (!view_.IsNull()) {
        return;
    }
    const Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    const Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(display);
    const Handle(V3d_Viewer) viewer = new V3d_Viewer(driver);
    context_ = new AIS_InteractiveContext(viewer);
    view_ = viewer->CreateView();
    const Handle(WNT_Window) window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
    view_->SetWindow(window);
    if (!window->IsMapped()) {
        window->Map();
    }
    view_->SetBackgroundColor(Quantity_Color(0.08, 0.12, 0.17, Quantity_TOC_RGB));
    view_->SetProj(V3d_Zpos);
    view_->MustBeResized();
}

void CadView::draw(const cadcam::CurrentZone& zone)
{
    initialize();
    context_->RemoveAll(false);
    for (const RenderItem& item : zone.items) {
        const gp_Trsf transform = placement(item);
        Handle(AIS_InteractiveObject) displayObject = std::visit([&](const auto& value) -> Handle(AIS_InteractiveObject) {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Text>) {
                Handle(AIS_TextLabel) label = new AIS_TextLabel();
                label->SetText(TCollection_ExtendedString(value.utf8.c_str(), true));
                label->SetPosition(point(value.insertion).Transformed(transform));
                label->SetHeight(value.height);
                if (!value.font.empty()) {
                    label->SetFont(value.font.c_str());
                }
                label->SetAngle((value.rotationDegrees + item.rotationDegrees) * Pi / 180.0);
                return label;
            } else {
                const TopoDS_Shape shape = makeShape(value);
                if (shape.IsNull()) {
                    return {};
                }
                Handle(AIS_Shape) presentation = new AIS_Shape(shape.Moved(TopLoc_Location(transform)));
                return presentation;
            }
        }, item.geometry);
        if (displayObject.IsNull()) {
            continue;
        }
        const Quantity_Color tint = item.role == RenderRole::SheetBoundary
            ? Quantity_Color(0.43, 0.58, 0.72, Quantity_TOC_RGB)
            : Quantity_Color(0.31, 0.89, 0.78, Quantity_TOC_RGB);
        context_->SetColor(displayObject, tint, false);
        context_->Display(displayObject, false);
    }
    if (!zone.items.empty()) {
        view_->FitAll(0.12, false);
        view_->ZFitAll();
    }
    view_->Redraw();
}

QPaintEngine* CadView::paintEngine() const
{
    return nullptr;
}

void CadView::paintEvent(QPaintEvent*)
{
    if (!view_.IsNull()) {
        view_->Redraw();
    }
}

void CadView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!view_.IsNull()) {
        view_->MustBeResized();
    }
}

void CadView::wheelEvent(QWheelEvent* event)
{
    if (!view_.IsNull()) {
        view_->SetZoom(std::pow(1.15, event->angleDelta().y() / 120.0));
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
        view_->Pan(delta.x(), -delta.y());
        previousMouse_ = next;
    }
}
