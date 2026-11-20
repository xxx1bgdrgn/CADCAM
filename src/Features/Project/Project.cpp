#include "Features/Project/Project.h"

namespace cadcam {

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
