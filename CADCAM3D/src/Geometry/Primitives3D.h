#pragma once

#include "Shape/Primitives.h"
#include "Shape/Shape.h"

#include <TopoDS_Wire.hxx>

#include <array>
#include <string>

namespace cadcam3d {

struct Point { double x; double y; double z; };
struct Direction { double x; double y; double z; };
struct Plane { Point origin; Direction normal; Direction xDirection; };
struct Circle { Plane plane; double radius; };
struct Rectangle { std::array<Point, 4> vertices; };
struct RoundedRectangle { Plane plane; double width; double height; double radius; };
struct Text
{
    Plane plane;
    std::string utf8;
    std::string font;
    double height;
    TopoDS_Shape surfaceContours;
};
struct Ellipse { Plane plane; double majorRadius; double minorRadius; };
struct EllipticalArc { Ellipse ellipse; double startParameter; double endParameter; };

using Primitive = cadcam::PrimitiveGeometry<Point, Circle, Rectangle, RoundedRectangle, Text,
    Ellipse, EllipticalArc, TopoDS_Wire>;
using Shape = cadcam::ShapeRecord<Primitive>;

}
