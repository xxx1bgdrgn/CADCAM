#pragma once

#include "Geometry/Primitives.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cadcam {

using Id = std::uint64_t;

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

Project exampleProject();

}
