#pragma once

#include "Features/Project/TubePart.h"

class TopoDS_Face;

namespace cadcam3d {

struct BoxProfile
{
    double width;
    double height;
    double wall;
    double length;
    double outerRadius;
};

struct SectionDimensions
{
    double width = 60;
    double height = 40;
    double wall = 2;
    double length = 240;
    double radius = 0;
    double flange = 4;
    double innerWidth = 56;
    double innerHeight = 36;
};

// The section is centered on YZ, with its near end at X = 0.
TubePart makeBoxTube(const BoxProfile& profile);
TubePart makeTube(TubeType type, const SectionDimensions& dimensions);
TopoDS_Face makeSection(TubeType type, const SectionDimensions& dimensions);

}
