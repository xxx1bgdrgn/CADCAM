#pragma once

#include "Features/Project/Project.h"

#include <vector>

namespace cadcam {

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

}
