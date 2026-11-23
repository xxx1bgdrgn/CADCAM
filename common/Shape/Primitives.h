#pragma once

#include <variant>
#include <vector>

namespace cadcam {

template<class Point>
struct LineGeometry { Point start; Point end; };

template<class Point>
struct ArcGeometry { Point start; Point through; Point end; };

template<class Point>
using SegmentGeometry = std::variant<LineGeometry<Point>, ArcGeometry<Point>>;

template<class Point>
struct PolylineGeometry { std::vector<SegmentGeometry<Point>> segments; bool closed; };

template<class Point, class Circle, class Rectangle, class RoundedRectangle,
    class Text, class Ellipse, class EllipticalArc, class... Extra>
using PrimitiveGeometry = std::variant<Point, LineGeometry<Point>, ArcGeometry<Point>, Circle,
    Rectangle, RoundedRectangle, Text, Ellipse, EllipticalArc, PolylineGeometry<Point>, Extra...>;

}
