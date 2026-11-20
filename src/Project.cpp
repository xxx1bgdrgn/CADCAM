#include "Project.h"

#include <algorithm>
#include <iterator>

namespace cadcam {
namespace {

template<class Record>
const Record& findById(const std::vector<Record>& records, Id id, const char* message)
{
    const auto found = std::find_if(records.begin(), records.end(),
        [id](const Record& record) { return record.id == id; });
    if (found == records.end()) {
        throw std::invalid_argument(message);
    }
    return *found;
}

void appendShapes(CurrentZone& zone, const std::vector<Shape>& shapes, const Placement& placement, Id groupId = 0)
{
    for (const Shape& shape : shapes) {
        if (const auto* primitive = std::get_if<Primitive>(&shape.content)) {
            zone.items.push_back({*primitive, RenderRole::Geometry, shape.id, groupId,
                placement.id, placement.offset, placement.rotationDegrees});
        } else {
            // Keep the top-level group selectable while retaining each leaf ID.
            appendShapes(zone, std::get<std::vector<Shape>>(shape.content), placement, groupId ? groupId : shape.id);
        }
    }
}

void appendSheet(CurrentZone& zone, const Sheet& sheet)
{
    zone.items.push_back({Rectangle{{0, 0}, sheet.size.x, sheet.size.y}, RenderRole::SheetBoundary});
}

}

CurrentZone buildZone(const Project& project, ZoneSource source)
{
    CurrentZone zone{source, {}};
    const Placement origin{0, 0, {0, 0}, 0};
    switch (source.kind) {
    case ZoneKind::BaseDrawing:
        if (project.nestingResults.empty() || !std::holds_alternative<BaseDrawing>(project.nestingResults.front())) {
            throw std::invalid_argument("Missing base drawing at index zero");
        }
        appendShapes(zone, std::get<BaseDrawing>(project.nestingResults.front()).shapes, origin);
        break;
    case ZoneKind::Part:
        appendShapes(zone, findById(project.parts, source.id, "Unknown part").shapes, origin);
        break;
    case ZoneKind::Sheet:
        appendSheet(zone, findById(project.sheets, source.id, "Unknown sheet"));
        break;
    case ZoneKind::NestingResult: {
        const auto& results = project.nestingResults;
        const auto begin = results.empty() ? results.end() : std::next(results.begin());
        const auto found = std::find_if(begin, results.end(), [source](const auto& result) {
            const auto* sheet = std::get_if<NestedSheet>(&result);
            return sheet && sheet->id == source.id;
        });
        if (found == results.end()) {
            throw std::invalid_argument("Unknown nesting result");
        }

        const NestedSheet& layout = std::get<NestedSheet>(*found);
        appendSheet(zone, findById(project.sheets, layout.sheetId, "Unknown sheet in nesting result"));
        for (const Placement& placement : layout.placements) {
            const Part& part = findById(project.parts, placement.partId, "Unknown part in nesting result");
            appendShapes(zone, part.shapes, placement);
        }
        break;
    }
    }
    return zone;
}

Project exampleProject()
{
    const Shape outline{1, Primitive{RoundedRectangle{{{0, 0}, 120, 70}, 8}},
        {Compensation{CompensationSide::Outside, 0.10}}};
    const Shape hole{2, Primitive{Circle{{30, 35}, 9}},
        {Compensation{CompensationSide::Inside, 0.10}, LeadIn{LeadStyle::Line, 2, 45}}};
    const Shape slot{3, Primitive{Ellipse{{85, 35}, 15, 8, 15}}, {}};
    const Shape group{4, std::vector<Shape>{hole, slot, outline}, {}};

    Project project;
    std::get<BaseDrawing>(project.nestingResults.front()).shapes = {group};
    project.parts.push_back({11, "安装板", {group}});
    project.sheets.push_back({21, "板材 A · 340 × 180 mm", {340, 180}});
    project.nestingResults.push_back(NestedSheet{31, "排样 1", 21, {
        {41, 11, {20, 19}, 0},
        {42, 11, {320, 159}, 180}
    }});
    return project;
}

}
