#pragma once

#include "Shape/Primitives.h"

#include <string>

namespace cadcam {

struct Point { double x; double y; };
using Line = LineGeometry<Point>;
using CircularArc = ArcGeometry<Point>;
struct Circle { Point center; double radius; };
struct Rectangle { Point lowerLeft; double width; double height; };
struct RoundedRectangle { Rectangle boundary; double radius; };
struct Text { Point insertion; std::string utf8; std::string font; double height; double rotationDegrees; };
struct Ellipse { Point center; double majorRadius; double minorRadius; double rotationDegrees; };
struct EllipticalArc { Ellipse ellipse; double startParameter; double endParameter; };
using PolylineSegment = SegmentGeometry<Point>;
using Polyline = PolylineGeometry<Point>;

using Primitive = PrimitiveGeometry<Point, Circle, Rectangle, RoundedRectangle, Text, Ellipse, EllipticalArc>;

}
