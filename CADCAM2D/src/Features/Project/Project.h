#pragma once

#include "Shape/Primitives2D.h"
#include "Shape/Shape.h"

#include <string>
#include <variant>
#include <vector>

namespace cadcam {

using Shape = ShapeRecord<Primitive>;

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
