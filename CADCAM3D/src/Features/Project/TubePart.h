#pragma once

#include "Geometry/Primitives3D.h"

#include <TopoDS_Shape.hxx>

#include <array>
#include <optional>

namespace cadcam3d {

enum class TubeType { BoxTube, RoundTube, OvalTube, IBeam, TSection, HBeam, CustomProfile,
    ObroundTube, AngleSteel, ChannelSteel };
enum class TubeSource { Step, Created };

struct TubeEnds
{
    std::array<TopoDS_Wire, 2> outlines;
    double length;
};

struct TubePart
{
    TubeSource source;
    std::optional<TubeType> type;
    TopoDS_Shape body;
    std::vector<Shape> shapes;
    std::optional<TubeEnds> ends;
};

}
