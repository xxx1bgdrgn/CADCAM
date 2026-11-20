#include "Rendering/CadView.h"

#include "Common/Angles.h"
#include "Features/Views/CurrentZone.h"
#include "Geometry/OcctGeometry.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <TCollection_ExtendedString.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <WNT_Window.hxx>
#include <gp_Trsf.hxx>

namespace {

using namespace cadcam;

gp_Trsf placement(const RenderItem& item)
{
    gp_Trsf transform;
    transform.SetRotation(gp::OZ(), degreesToRadians(item.rotationDegrees));
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
        Handle(AIS_InteractiveObject) displayObject;
        if (const auto* text = std::get_if<Text>(&item.geometry)) {
            Handle(AIS_TextLabel) label = new AIS_TextLabel();
            label->SetText(TCollection_ExtendedString(text->utf8.c_str(), true));
            label->SetPosition(toOcctPoint(text->insertion).Transformed(transform));
            label->SetHeight(text->height);
            if (!text->font.empty()) {
                label->SetFont(text->font.c_str());
            }
            label->SetAngle(degreesToRadians(text->rotationDegrees + item.rotationDegrees));
            displayObject = label;
        } else {
            const TopoDS_Shape shape = makeOcctShape(item.geometry);
            if (shape.IsNull()) {
                continue;
            }
            displayObject = new AIS_Shape(shape.Moved(TopLoc_Location(transform)));
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

void CadView::zoomBy(double factor)
{
    view_->SetZoom(factor);
}

void CadView::panBy(int x, int y)
{
    view_->Pan(x, y);
}
