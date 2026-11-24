#include "Rendering/TubeView.h"

#include "Features/Project/TubePart.h"
#include "Rendering/ContourColor.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <AIS_ViewCube.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <Bnd_Box.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <WNT_Window.hxx>

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

namespace cadcam3d {
namespace {

gp_Pnt toPoint(const Point& point)
{
    return {point.x, point.y, point.z};
}

void displayShape(const Handle(AIS_InteractiveContext)& context, const Shape& shape)
{
    if (const auto* children = std::get_if<std::vector<Shape>>(&shape.content)) {
        for (const auto& child : *children) {
            displayShape(context, child);
        }
        return;
    }

    const auto& primitive = std::get<Primitive>(shape.content);
    Handle(AIS_InteractiveObject) display;
    if (const auto* rectangle = std::get_if<Rectangle>(&primitive)) {
        BRepBuilderAPI_MakePolygon polygon;
        for (const auto& point : rectangle->vertices) {
            polygon.Add(toPoint(point));
        }
        polygon.Close();
        display = new AIS_Shape(polygon.Wire());
    } else if (const auto* wire = std::get_if<TopoDS_Wire>(&primitive)) {
        display = new AIS_Shape(*wire);
    } else if (const auto* point = std::get_if<Point>(&primitive)) {
        display = new AIS_Point(new Geom_CartesianPoint(toPoint(*point)));
    } else if (const auto* text = std::get_if<Text>(&primitive)) {
        if (!text->surfaceContours.IsNull()) {
            display = new AIS_Shape(text->surfaceContours);
        }
    }
    if (!display.IsNull()) {
        display->SetColor(cadcam::occtContourColor());
        display->SetWidth(std::holds_alternative<Text>(primitive) ? 1.0 : 2.0);
        display->SetZLayer(Graphic3d_ZLayerId_Top);
        context->Display(display, false);
    }
}

}

TubeView::TubeView(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMinimumSize(480, 360);
}

TubeView::~TubeView()
{
    if (!context_.IsNull()) {
        context_->RemoveAll(false);
    }
    if (!view_.IsNull()) {
        view_->Remove();
    }
}

void TubeView::initialize()
{
    if (!view_.IsNull()) {
        return;
    }
    const Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    const Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(display);
    const Handle(V3d_Viewer) viewer = new V3d_Viewer(driver);
    viewer->SetDefaultLights();
    viewer->SetLightOn();
    context_ = new AIS_InteractiveContext(viewer);
    context_->SetPixelTolerance(5);
    view_ = viewer->CreateView();
    const Handle(WNT_Window) window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
    view_->SetWindow(window);
    if (!window->IsMapped()) {
        window->Map();
    }
    view_->SetBackgroundColor(Quantity_Color(0.08, 0.10, 0.13, Quantity_TOC_sRGB));
    view_->SetProj(V3d_XnegYnegZpos);
    view_->MustBeResized();
}

void TubeView::draw(const TubePart& part)
{
    initialize();
    context_->RemoveAll(false);
    const Handle(AIS_Shape) body = new AIS_Shape(part.body);
    body->SetColor(Quantity_Color(0.61, 0.72, 0.80, Quantity_TOC_sRGB));
    body->Attributes()->SetFaceBoundaryDraw(true);
    context_->Display(body, AIS_Shaded, 0, false);

    if (part.ends) {
        const auto endColor = cadcam::occtContourColor();
        for (std::size_t i = 0; i < part.ends->outlines.size(); ++i) {
            const auto& wire = part.ends->outlines[i];
            const Handle(AIS_Shape) outline = new AIS_Shape(wire);
            outline->SetColor(endColor);
            outline->SetWidth(2.0);
            outline->SetZLayer(Graphic3d_ZLayerId_Top);
            context_->Display(outline, AIS_WireFrame, -1, false);

            Bnd_Box bounds;
            BRepBndLib::Add(wire, bounds);
            const auto top = bounds.CornerMax();
            const auto bottom = bounds.CornerMin();
            const Handle(AIS_TextLabel) label = new AIS_TextLabel();
            label->SetText(TCollection_ExtendedString(i == 0 ? "1" : "2"));
            label->SetPosition(gp_Pnt(i == 0 ? 0 : part.ends->length, (top.Y() + bottom.Y()) / 2,
                top.Z() + 0.12 * (top.Z() - bottom.Z())));
            label->SetColor(endColor);
            label->SetHeight(16);
            label->SetInfiniteState(true);
            label->SetZLayer(Graphic3d_ZLayerId_Topmost);
            context_->Display(label, 0, -1, false);
        }
    }

    for (const auto& shape : part.shapes) {
        displayShape(context_, shape);
    }

    const Handle(AIS_ViewCube) cube = new AIS_ViewCube();
    const bool compact = width() < 340;
    cube->SetSize(compact ? 33 : 49);
    cube->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_TriedronPers,
        Aspect_TOTP_RIGHT_LOWER, compact ? Graphic3d_Vec2i(54, 51) : Graphic3d_Vec2i(82, 73)));
    context_->Display(cube, false);

    view_->FitAll(0.12, false);
    view_->ZFitAll();
    view_->Redraw();
}

QPaintEngine* TubeView::paintEngine() const
{
    return nullptr;
}

void TubeView::paintEvent(QPaintEvent*)
{
    if (!view_.IsNull()) {
        view_->Redraw();
    }
}

void TubeView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!view_.IsNull()) {
        view_->MustBeResized();
    }
}

void TubeView::wheelEvent(QWheelEvent* event)
{
    if (!view_.IsNull()) {
        view_->SetZoom(std::pow(1.15, event->angleDelta().y() / 120.0));
    }
    event->accept();
}

void TubeView::mousePressEvent(QMouseEvent* event)
{
    if (view_.IsNull()) {
        return;
    }
    previousMouse_ = (event->position() * devicePixelRatioF()).toPoint();
    if (event->button() == Qt::MiddleButton) {
        view_->StartRotation(previousMouse_.x(), previousMouse_.y());
    } else if (event->button() == Qt::LeftButton) {
        context_->MoveTo(previousMouse_.x(), previousMouse_.y(), view_, false);
        const bool viewPicked = context_->HasDetected() &&
            !Handle(AIS_ViewCube)::DownCast(context_->DetectedInteractive()).IsNull();
        context_->SelectDetected();
        context_->ClearDetected();
        if (viewPicked) {
            view_->FitAll(0.12, false);
            view_->ZFitAll();
        }
        view_->Redraw();
    }
}

void TubeView::mouseMoveEvent(QMouseEvent* event)
{
    if (view_.IsNull()) {
        return;
    }
    const QPoint next = (event->position() * devicePixelRatioF()).toPoint();
    if (event->buttons().testFlag(Qt::MiddleButton)) {
        view_->Rotation(next.x(), next.y());
    } else if (event->buttons().testFlag(Qt::RightButton)) {
        const QPoint delta = next - previousMouse_;
        view_->Pan(delta.x(), -delta.y());
    }
    previousMouse_ = next;
}

}
