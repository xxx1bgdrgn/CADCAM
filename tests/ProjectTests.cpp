#include "Project.h"

#include <cstdlib>
#include <iostream>

using namespace cadcam;

void require(bool passed, const char* message)
{
    if (!passed) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    Project project;
    require(project.parts.empty() && project.sheets.empty(), "New lists must be empty");
    require(project.nestingResults.size() == 1, "New project must contain exactly one base drawing");
    require(buildZone(project, {ZoneKind::BaseDrawing, 0}).items.empty(), "New base drawing must be empty");

    const Shape circle{11, Primitive{Circle{{5, 5}, 2}}, {Compensation{CompensationSide::Inside, 0.1}}};
    const Shape line{12, Primitive{Line{{0, 0}, {5, 0}}}, {}};
    const Shape group{13, std::vector<Shape>{circle, line}, {}};
    std::get<BaseDrawing>(project.nestingResults.front()).shapes.push_back(group);
    project.parts.push_back({20, "Part", {group}});
    project.sheets.push_back({30, "Sheet", {100, 60}});
    project.nestingResults.push_back(NestedSheet{40, "Layout", 30, {
        {51, 20, {10, 15}, 90},
        {52, 20, {40, 25}, 0}
    }});

    const CurrentZone base = buildZone(project, {ZoneKind::BaseDrawing, 0});
    require(base.items.size() == 2, "Base drawing must render both group members");
    require(base.items[0].shapeId == 11 && base.items[1].shapeId == 12, "Group member order must be preserved");
    require(base.items[0].groupId == 13, "Group selection must identify the owning group");

    const CurrentZone part = buildZone(project, {ZoneKind::Part, 20});
    require(part.items.size() == 2, "Part must display its own shapes");
    const CurrentZone sheet = buildZone(project, {ZoneKind::Sheet, 30});
    require(sheet.items.size() == 1 && sheet.items.front().role == RenderRole::SheetBoundary,
        "Sheet view must display its boundary");
    const CurrentZone layout = buildZone(project, {ZoneKind::NestingResult, 40});
    require(layout.items.size() == 5, "Layout must display the sheet and both placed instances");
    require(layout.items[1].placementId == 51 && layout.items[3].placementId == 52,
        "Instances of the same part must have distinct IDs");
    require(layout.items[1].offset.x == 10 && layout.items[1].offset.y == 15 && layout.items[1].rotationDegrees == 90,
        "Instance transform must match the saved placement");

    try {
        buildZone(project, {ZoneKind::Part, 999});
        require(false, "Unknown view must report an error");
    } catch (const std::invalid_argument&) {
    }
    project.parts.clear();
    try {
        buildZone(project, {ZoneKind::NestingResult, 40});
        require(false, "Missing parts must not create a partial layout");
    } catch (const std::invalid_argument&) {
    }
}
