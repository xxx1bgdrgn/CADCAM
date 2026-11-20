#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace cadcam {

using Id = std::uint64_t;

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

enum class CompensationSide { Inside, Outside };
struct Compensation { CompensationSide side; double distance; };
enum class LeadStyle { Line, Arc };
struct LeadIn { LeadStyle style; double length; double angleDegrees; };
struct LeadOut { LeadStyle style; double length; double angleDegrees; };
using Operation = std::variant<Compensation, LeadIn, LeadOut>;

struct Shape
{
    Id id;
    std::variant<Primitive, std::vector<Shape>> content;
    std::vector<Operation> operations;
};

struct Part { Id id; std::string name; std::vector<Shape> shapes; };
struct Sheet { Id id; std::string name; Point size; };
struct BaseDrawing { std::vector<Shape> shapes; };
struct Placement { Id id; Id partId; Point offset; double rotationDegrees; };
struct NestedSheet { Id id; std::string name; Id sheetId; std::vector<Placement> placements; };

struct Project
{
    std::vector<Part> parts;
    std::vector<Sheet> sheets;
    // Slot zero is reserved for the imported geometry or a new empty drawing.
    std::vector<std::variant<BaseDrawing, NestedSheet>> nestingResults{BaseDrawing{}};
};

enum class ZoneKind { BaseDrawing, Part, Sheet, NestingResult };
struct ZoneSource { ZoneKind kind; Id id; };

enum class RenderRole { Geometry, SheetBoundary };

struct RenderItem
{
    Primitive geometry;
    RenderRole role;
    Id shapeId = 0;
    Id groupId = 0;
    Id placementId = 0;
    Point offset{0, 0};
    double rotationDegrees = 0;
};

struct CurrentZone
{
    ZoneSource source;
    std::vector<RenderItem> items;
};

CurrentZone buildZone(const Project& project, ZoneSource source);
Project exampleProject();

}
