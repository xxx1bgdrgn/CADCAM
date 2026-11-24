#include "Rendering/CadView.h"

#include "Common/Angles.h"
#include "Features/Views/CurrentZone.h"
#include "Rendering/ContourColor.h"
#include "Shape/OcctGeometry2D.h"

#include <AIS_ConnectedInteractive.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_MultipleConnectedInteractive.hxx>
#include <AIS_RubberBand.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Aspect_Grid.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <Graphic3d_AspectMarker3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_Structure.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <SelectMgr_ViewerSelector.hxx>
#include <TColStd_HArray1OfByte.hxx>
#include <TCollection_ExtendedString.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <WNT_Window.hxx>
#include <gp_Trsf.hxx>

#include <map>
#include <utility>
#include <vector>

namespace {

using namespace cadcam;

gp_Trsf placement(const RenderItem& item)
{
    gp_Trsf transform;
    transform.SetRotation(gp::OZ(), degreesToRadians(item.rotationDegrees));
    transform.SetTranslationPart(gp_Vec(item.offset.x, item.offset.y, 0));
    return transform;
}

void setSelectionLineStyle(const Handle(PrsMgr_PresentableObject)& object, bool selected)
{
    if (const auto shape = Handle(AIS_Shape)::DownCast(object)) {
        const auto& drawer = shape->Attributes();
        const auto type = selected ? Aspect_TOL_DASH : Aspect_TOL_SOLID;
        for (const auto& line : {drawer->LineAspect(), drawer->WireAspect(),
                 drawer->FreeBoundaryAspect(), drawer->UnFreeBoundaryAspect()}) {
            line->SetTypeOfLine(type);
        }
        shape->SynchronizeAspects();
    } else if (const auto instance = Handle(AIS_ConnectedInteractive)::DownCast(object)) {
        setSelectionLineStyle(instance->ConnectedTo(), selected);
    } else {
        for (const auto& child : object->Children()) {
            setSelectionLineStyle(child, selected);
        }
    }
}

void drawSelectionGrips(const Handle(Graphic3d_Structure)& grips, const Bnd_Box& bounds)
{
    grips->Clear();
    if (bounds.IsVoid()) {
        grips->Erase();
        return;
    }

    const gp_Pnt lower = bounds.CornerMin();
    const gp_Pnt upper = bounds.CornerMax();
    const double centerX = (lower.X() + upper.X()) * 0.5;
    const double centerY = (lower.Y() + upper.Y()) * 0.5;
    const Quantity_Color gray(0.62, 0.64, 0.68, Quantity_TOC_sRGB);
    // An eight-pixel hollow square keeps the grips readable as the view zooms.
    const Handle(TColStd_HArray1OfByte) bitmap = new TColStd_HArray1OfByte(1, 8, 0x81);
    bitmap->SetValue(1, 0xff);
    bitmap->SetValue(8, 0xff);
    const Handle(Graphic3d_ArrayOfPoints) edges = new Graphic3d_ArrayOfPoints(8);
    for (const double x : {lower.X(), centerX, upper.X()}) {
        edges->AddVertex(x, lower.Y(), 0);
        edges->AddVertex(x, upper.Y(), 0);
    }
    edges->AddVertex(lower.X(), centerY, 0);
    edges->AddVertex(upper.X(), centerY, 0);
    const auto edgeGroup = grips->NewGroup();
    edgeGroup->SetGroupPrimitivesAspect(new Graphic3d_AspectMarker3d(gray, 8, 8, bitmap));
    edgeGroup->AddPrimitiveArray(edges);

    const Handle(Graphic3d_ArrayOfPoints) center = new Graphic3d_ArrayOfPoints(1);
    center->AddVertex(centerX, centerY, 0);
    const auto centerGroup = grips->NewGroup();
    centerGroup->SetGroupPrimitivesAspect(new Graphic3d_AspectMarker3d(Aspect_TOM_O, gray, 1.0));
    centerGroup->AddPrimitiveArray(center);
    grips->Display();
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
    if (!selectionGrips_.IsNull()) {
        // Detach the grips from the view before freeing their graphics structure.
        selectionGrips_->Erase();
        selectionGrips_->Remove();
    }
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
    view_->SetBackgroundColor(Quantity_NOC_BLACK);
    view_->SetProj(V3d_Zpos);
    viewer->SetRectangularGridValues(0, 0, 10, 10, 0);
    // Keep the grid behind the XY plane so contours stay visible over it.
    viewer->SetRectangularGridGraphicValues(5000, 5000, 0.2);
    viewer->Grid(Aspect_GT_Rectangular)->SetColors(
        Quantity_Color(0.20, 0.20, 0.20, Quantity_TOC_sRGB),
        Quantity_Color(0.30, 0.30, 0.30, Quantity_TOC_sRGB));
    viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    viewer->SetGridEcho(false);
    context_->SetPixelTolerance(5);
    context_->SelectionStyle()->SetColor(occtContourColor());
    selectionBox_ = new AIS_RubberBand();
    selectionBox_->SetTransformPersistence(
        new Graphic3d_TransformPers(Graphic3d_TMF_2d, Aspect_TOTP_LEFT_UPPER));
    selectionBox_->SetMutable(true);
    selectionGrips_ = new Graphic3d_Structure(viewer->StructureManager());
    selectionGrips_->SetZLayer(Graphic3d_ZLayerId_Topmost);
    selectionGrips_->SetInfiniteState(true);
    view_->MustBeResized();
}

void CadView::draw(const cadcam::CurrentZone& zone)
{
    initialize();
    selectionStart_.reset();
    draggingSelection_ = false;
    styledSelection_.clear();
    selectionGrips_->Erase();
    selectionGrips_->Clear();
    context_->RemoveAll(false);
    std::map<std::pair<Id, Id>, Handle(AIS_MultipleConnectedInteractive)> groups;
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
            : occtContourColor();
        displayObject->SetColor(tint);
        if (item.role == RenderRole::SheetBoundary) {
            context_->Display(displayObject, 0, -1, false);
        } else if (item.groupId != 0) {
            auto& group = groups[{item.placementId, item.groupId}];
            if (group.IsNull()) {
                group = new AIS_MultipleConnectedInteractive();
            }
            group->Connect(displayObject);
        } else {
            context_->Display(displayObject, false);
        }
    }
    for (const auto& [key, group] : groups) {
        context_->Display(group, false);
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

void CadView::showSelectionBox(QPoint start, QPoint end)
{
    const QRect rect = QRect(start, end).normalized();
    const bool crossing = end.x() < start.x();
    const Quantity_Color color = crossing
        ? Quantity_Color(0.33, 0.85, 0.57, Quantity_TOC_sRGB)
        : Quantity_Color(0.35, 0.66, 1.0, Quantity_TOC_sRGB);
    selectionBox_->SetLineColor(color);
    selectionBox_->SetLineType(crossing ? Aspect_TOL_DASH : Aspect_TOL_SOLID);
    selectionBox_->SetFilling(color, 0.86);
    selectionBox_->SetRectangle(rect.left(), -qMax(rect.bottom(), rect.top() + 1),
        qMax(rect.right(), rect.left() + 1), -rect.top());
    selectionBox_->SetToUpdate();
    context_->Display(selectionBox_, 0, -1, false);
    view_->InvalidateImmediate();
    view_->RedrawImmediate();
}

void CadView::selectAt(QPoint position)
{
    context_->Remove(selectionBox_, false);
    context_->MoveTo(position.x(), position.y(), view_, false);
    context_->SelectDetected();
    context_->ClearDetected();
    refreshSelectionAppearance();
}

void CadView::selectBox(QPoint start, QPoint end)
{
    context_->Remove(selectionBox_, false);
    const QRect rect = QRect(start, end).normalized();
    if (rect.width() == 1 || rect.height() == 1) {
        context_->ClearSelected(false);
        refreshSelectionAppearance();
        return;
    }
    const auto& selector = context_->MainSelector();
    const bool crossing = end.x() < start.x();
    selector->AllowOverlapDetection(crossing);
    context_->SelectRectangle(Graphic3d_Vec2i(rect.left(), rect.top()),
        Graphic3d_Vec2i(rect.right(), rect.bottom()), view_);
    selector->AllowOverlapDetection(false);
    if (!crossing) {
        excludePartialGroups(rect);
    }
    refreshSelectionAppearance();
}

void CadView::refreshSelectionAppearance()
{
    for (const auto& object : styledSelection_) {
        if (!context_->IsSelected(object)) {
            setSelectionLineStyle(object, false);
        }
    }
    styledSelection_.clear();
    Bnd_Box selectionBounds;
    for (context_->InitSelected(); context_->MoreSelected(); context_->NextSelected()) {
        const auto object = context_->SelectedInteractive();
        styledSelection_.push_back(object);
        setSelectionLineStyle(object, true);
        Bnd_Box objectBounds;
        object->BoundingBox(objectBounds);
        selectionBounds.Add(objectBounds);
    }
    drawSelectionGrips(selectionGrips_, selectionBounds);
    view_->Redraw();
}

void CadView::excludePartialGroups(const QRect& rect)
{
    // OCCT can return a group when only one of its presentations is inside the window.
    std::vector<Handle(SelectMgr_EntityOwner)> partial;
    for (context_->InitSelected(); context_->MoreSelected(); context_->NextSelected()) {
        const auto group = Handle(AIS_MultipleConnectedInteractive)::DownCast(context_->SelectedInteractive());
        if (group.IsNull()) {
            continue;
        }
        Bnd_Box bounds;
        group->BoundingBox(bounds);
        if (bounds.IsVoid()) {
            continue;
        }
        const gp_Pnt lower = bounds.CornerMin();
        const gp_Pnt upper = bounds.CornerMax();
        QPoint first, last;
        view_->Convert(lower.X(), lower.Y(), lower.Z(), first.rx(), first.ry());
        view_->Convert(upper.X(), upper.Y(), upper.Z(), last.rx(), last.ry());
        if (!rect.contains(QRect(first, last).normalized())) {
            partial.push_back(context_->SelectedOwner());
        }
    }
    for (const auto& owner : partial) {
        context_->AddOrRemoveSelected(owner, false);
    }
}
