#pragma once

#include <string>
#include <variant>
#include <vector>

namespace cadcam {

struct Point { double x; double y; };
struct Line { Point start; Point end; };
struct CircularArc { Point start; Point through; Point end; };
struct Circle { Point center; double radius; };
struct Rectangle { Point lowerLeft; double width; double height; };
struct RoundedRectangle { Rectangle boundary; double radius; };
struct Text { Point insertion; std::string utf8; std::string font; double height; double rotationDegrees; };
struct Ellipse { Point center; double majorRadius; double minorRadius; double rotationDegrees; };
struct EllipticalArc { Ellipse ellipse; double startParameter; double endParameter; };
using PolylineSegment = std::variant<Line, CircularArc>;
struct Polyline { std::vector<PolylineSegment> segments; bool closed; };

using Primitive = std::variant<Point, Line, CircularArc, Circle, Rectangle,
    RoundedRectangle, Text, Ellipse, EllipticalArc, Polyline>;

}
