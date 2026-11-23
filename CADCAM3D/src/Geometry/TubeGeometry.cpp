#include "Geometry/TubeGeometry.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Precision.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cadcam3d {
namespace {

TopoDS_Wire polygon(std::initializer_list<std::pair<double, double>> points)
{
    BRepBuilderAPI_MakePolygon builder;
    for (const auto& [y, z] : points) {
        builder.Add(gp_Pnt(0, y, z));
    }
    builder.Close();
    return builder.Wire();
}

TopoDS_Wire boxBoundary(double width, double height, double radius)
{
    const double left = -width / 2;
    const double right = width / 2;
    const double bottom = -height / 2;
    const double top = height / 2;
    if (radius <= Precision::Confusion()) {
        return BRepBuilderAPI_MakePolygon(gp_Pnt(0, left, bottom), gp_Pnt(0, right, bottom),
            gp_Pnt(0, right, top), gp_Pnt(0, left, top), true).Wire();
    }

    const double inset = radius * (1 - std::sqrt(0.5));
    BRepBuilderAPI_MakeWire wire;
    const auto line = [&](const gp_Pnt& from, const gp_Pnt& to) {
        wire.Add(BRepBuilderAPI_MakeEdge(from, to).Edge());
    };
    const auto arc = [&](const gp_Pnt& from, const gp_Pnt& through, const gp_Pnt& to) {
        wire.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(from, through, to).Value()).Edge());
    };
    line({0, left + radius, bottom}, {0, right - radius, bottom});
    arc({0, right - radius, bottom}, {0, right - inset, bottom + inset}, {0, right, bottom + radius});
    line({0, right, bottom + radius}, {0, right, top - radius});
    arc({0, right, top - radius}, {0, right - inset, top - inset}, {0, right - radius, top});
    line({0, right - radius, top}, {0, left + radius, top});
    arc({0, left + radius, top}, {0, left + inset, top - inset}, {0, left, top - radius});
    line({0, left, top - radius}, {0, left, bottom + radius});
    arc({0, left, bottom + radius}, {0, left + inset, bottom + inset}, {0, left + radius, bottom});
    return wire.Wire();
}

TopoDS_Wire ellipseBoundary(double width, double height)
{
    const auto& axis = width >= height ? gp::DY() : gp::DZ();
    const gp_Ax2 axes(gp::Origin(), gp::DX(), axis);
    const auto edge = std::abs(width - height) <= Precision::Confusion()
        ? BRepBuilderAPI_MakeEdge(gp_Circ(axes, width / 2)).Edge()
        : BRepBuilderAPI_MakeEdge(gp_Elips(axes, std::max(width, height) / 2, std::min(width, height) / 2)).Edge();
    return BRepBuilderAPI_MakeWire(edge).Wire();
}

TopoDS_Wire obroundBoundary(double width, double height)
{
    const double center = (width - height) / 2;
    const double radius = height / 2;
    const gp_Pnt a(0, -center, -radius), b(0, center, -radius);
    const gp_Pnt c(0, center, radius), d(0, -center, radius);
    BRepBuilderAPI_MakeWire builder;
    builder.Add(BRepBuilderAPI_MakeEdge(a, b).Edge());
    builder.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(b, gp_Pnt(0, center + radius, 0), c).Value()).Edge());
    builder.Add(BRepBuilderAPI_MakeEdge(c, d).Edge());
    builder.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(d, gp_Pnt(0, -center - radius, 0), a).Value()).Edge());
    return builder.Wire();
}

TopoDS_Face materialFace(const TopoDS_Wire& outer, const TopoDS_Wire& inner = {})
{
    BRepBuilderAPI_MakeFace builder(gp_Pln(gp::Origin(), gp::DX()), outer);
    if (!inner.IsNull()) {
        builder.Add(TopoDS::Wire(inner.Reversed()));
    }
    const auto section = builder.Face();
    if (!BRepCheck_Analyzer(section).IsValid()) {
        throw std::invalid_argument("截面无效，请检查尺寸和内外轮廓。");
    }
    return section;
}

}

TopoDS_Face makeSection(TubeType type, const SectionDimensions& dimensions)
{
    const auto [width, height, wall, length, outerRadius, flange, innerWidth, innerHeight] = dimensions;
    for (double value : {width, height, wall, length, outerRadius, flange, innerWidth, innerHeight}) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("截面尺寸必须是有限数值。");
        }
    }
    const double tol = Precision::Confusion();
    const double halfWidth = width / 2;
    const double halfHeight = height / 2;
    if (std::min(width, height) <= tol) {
        throw std::invalid_argument("外部截面尺寸必须大于零。");
    }
    if (type == TubeType::OvalTube) {
        if (std::min(innerWidth, innerHeight) <= tol || width - innerWidth <= tol || height - innerHeight <= tol) {
            throw std::invalid_argument("椭圆管的内轮廓尺寸必须大于零且小于对应的外部尺寸。");
        }
        return materialFace(ellipseBoundary(width, height), ellipseBoundary(innerWidth, innerHeight));
    }
    if (type == TubeType::BoxTube || type == TubeType::RoundTube || type == TubeType::ObroundTube) {
        const double shortSide = type == TubeType::RoundTube ? width : std::min(width, height);
        if (wall <= tol || shortSide / 2 - wall <= tol) {
            throw std::invalid_argument("壁厚必须大于零，并给内腔留出空间。");
        }
        if (type == TubeType::RoundTube) {
            return materialFace(ellipseBoundary(width, width), ellipseBoundary(width - 2 * wall, width - 2 * wall));
        }
        if (type == TubeType::ObroundTube) {
            if (width - height <= tol) {
                throw std::invalid_argument("腰形管的总宽必须大于总高。");
            }
            return materialFace(obroundBoundary(width, height), obroundBoundary(width - 2 * wall, height - 2 * wall));
        }
        if (outerRadius < 0 || (outerRadius > 0 && (outerRadius < wall || shortSide / 2 - outerRadius <= tol))) {
            throw std::invalid_argument("外圆角半径应为零，或不小于壁厚且小于短边的一半。");
        }
        return materialFace(boxBoundary(width, height, outerRadius),
            boxBoundary(width - 2 * wall, height - 2 * wall, std::max(0.0, outerRadius - wall)));
    }

    const double left = -halfWidth, right = halfWidth, bottom = -halfHeight, top = halfHeight;
    if (type == TubeType::AngleSteel) {
        if (wall <= tol || std::min(width, height) - wall <= tol) {
            throw std::invalid_argument("角钢厚度必须小于两条边的长度。");
        }
        return materialFace(polygon({{left, bottom}, {right, bottom}, {right, bottom + wall},
            {left + wall, bottom + wall}, {left + wall, top}, {left, top}}));
    }
    if (type == TubeType::CustomProfile) {
        throw std::invalid_argument("异型管需要导入 STEP 或提供手绘截面。");
    }
    const double flangeCount = type == TubeType::TSection ? 1 : 2;
    if (wall <= tol || width - wall <= tol || flange <= tol || height - flangeCount * flange <= tol) {
        throw std::invalid_argument("腹板或翼缘厚度超出截面尺寸。");
    }
    const double w = wall / 2;
    if (type == TubeType::ChannelSteel) {
        return materialFace(polygon({{left, bottom}, {right, bottom}, {right, bottom + flange},
            {left + wall, bottom + flange}, {left + wall, top - flange}, {right, top - flange},
            {right, top}, {left, top}}));
    }
    if (type == TubeType::TSection) {
        return materialFace(polygon({{-w, bottom}, {w, bottom}, {w, top - flange}, {right, top - flange},
            {right, top}, {left, top}, {left, top - flange}, {-w, top - flange}}));
    }
    return materialFace(polygon({{left, bottom}, {right, bottom}, {right, bottom + flange},
        {w, bottom + flange}, {w, top - flange}, {right, top - flange}, {right, top},
        {left, top}, {left, top - flange}, {-w, top - flange}, {-w, bottom + flange}, {left, bottom + flange}}));
}

TubePart makeTube(TubeType type, const SectionDimensions& dimensions)
{
    if (!std::isfinite(dimensions.length) || dimensions.length <= Precision::Confusion()) {
        throw std::invalid_argument("管件长度必须大于零。");
    }
    const auto section = makeSection(type, dimensions);
    const gp_Vec travel(dimensions.length, 0, 0);
    const auto body = BRepPrimAPI_MakePrism(section, travel).Shape();
    if (body.IsNull() || body.ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer(body).IsValid()) {
        throw std::runtime_error("截面未能拉伸为有效的三维实体。");
    }
    const auto nearEnd = BRepTools::OuterWire(section);
    gp_Trsf placement;
    placement.SetTranslation(travel);
    const auto farEnd = TopoDS::Wire(nearEnd.Moved(TopLoc_Location(placement)));
    return {TubeSource::Created, type, body, {}, TubeEnds{{nearEnd, farEnd}, dimensions.length}};
}

TubePart makeBoxTube(const BoxProfile& profile)
{
    return makeTube(TubeType::BoxTube, {profile.width, profile.height, profile.wall, profile.length, profile.outerRadius});
}

}
