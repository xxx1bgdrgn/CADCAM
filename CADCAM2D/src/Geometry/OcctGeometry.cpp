#include "Geometry/OcctGeometry.h"

#include "Common/Angles.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>

#include <algorithm>
#include <cmath>

namespace cadcam {

gp_Pnt toOcctPoint(Point point)
{
    return {point.x, point.y, 0};
}

namespace {

TopoDS_Edge makeEdge(const Line& line)
{
    return BRepBuilderAPI_MakeEdge(toOcctPoint(line.start), toOcctPoint(line.end)).Edge();
}

TopoDS_Edge makeEdge(const CircularArc& arc)
{
    const auto curve = GC_MakeArcOfCircle(toOcctPoint(arc.start), toOcctPoint(arc.through), toOcctPoint(arc.end)).Value();
    return BRepBuilderAPI_MakeEdge(curve).Edge();
}

gp_Elips ellipseCurve(const Ellipse& ellipse)
{
    const double angle = degreesToRadians(ellipse.rotationDegrees);
    const gp_Ax2 axes(toOcctPoint(ellipse.center), gp::DZ(), gp_Dir(std::cos(angle), std::sin(angle), 0));
    return {axes, ellipse.majorRadius, ellipse.minorRadius};
}

TopoDS_Shape makeShape(const Point& marker)
{
    return BRepBuilderAPI_MakeVertex(toOcctPoint(marker)).Vertex();
}

TopoDS_Shape makeShape(const Line& line)
{
    return makeEdge(line);
}

TopoDS_Shape makeShape(const CircularArc& arc)
{
    return makeEdge(arc);
}

TopoDS_Shape makeShape(const Circle& circle)
{
    return BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(toOcctPoint(circle.center), gp::DZ()), circle.radius)).Edge();
}

TopoDS_Shape makeShape(const Rectangle& rectangle)
{
    const auto [x, y] = rectangle.lowerLeft;
    const double right = x + rectangle.width;
    const double top = y + rectangle.height;
    return BRepBuilderAPI_MakePolygon(toOcctPoint({x, y}), toOcctPoint({right, y}),
        toOcctPoint({right, top}), toOcctPoint({x, top}), true).Wire();
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
    wire.Add(makeEdge(Line{{x + r, y}, {right - r, y}}));
    wire.Add(makeEdge(CircularArc{{right - r, y}, {right - inset, y + inset}, {right, y + r}}));
    wire.Add(makeEdge(Line{{right, y + r}, {right, top - r}}));
    wire.Add(makeEdge(CircularArc{{right, top - r}, {right - inset, top - inset}, {right - r, top}}));
    wire.Add(makeEdge(Line{{right - r, top}, {x + r, top}}));
    wire.Add(makeEdge(CircularArc{{x + r, top}, {x + inset, top - inset}, {x, top - r}}));
    wire.Add(makeEdge(Line{{x, top - r}, {x, y + r}}));
    wire.Add(makeEdge(CircularArc{{x, y + r}, {x + inset, y + inset}, {x + r, y}}));
    return wire.Wire();
}

TopoDS_Shape makeShape(const Text&)
{
    // Editable text is rendered as a label until it is converted to a cutting outline.
    return {};
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
        wire.Add(std::visit([](const auto& value) { return makeEdge(value); }, segment));
    }
    if (polyline.closed && !polyline.segments.empty()) {
        const Point start = std::visit([](const auto& value) { return value.start; }, polyline.segments.front());
        const Point end = std::visit([](const auto& value) { return value.end; }, polyline.segments.back());
        if (std::hypot(start.x - end.x, start.y - end.y) > 1e-7) {
            wire.Add(makeEdge(Line{end, start}));
        }
    }
    return wire.IsDone() ? TopoDS_Shape(wire.Wire()) : TopoDS_Shape{};
}

}

TopoDS_Shape makeOcctShape(const Primitive& primitive)
{
    return std::visit([](const auto& value) { return makeShape(value); }, primitive);
}

}
