#include "Geometry/TubeUnfold.h"

#include "Common/Angles.h"
#include "Shape/OcctGeometry2D.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <QFont>
#include <QPolygonF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cadcam3d {
namespace {

constexpr double Tolerance = 1e-6;
constexpr double ChordTolerance = 0.02;

TopoDS_Wire asWire(const TopoDS_Shape& shape)
{
    if (shape.ShapeType() == TopAbs_EDGE) {
        return BRepBuilderAPI_MakeWire(TopoDS::Edge(shape)).Wire();
    }
    if (shape.ShapeType() != TopAbs_WIRE) {
        throw std::invalid_argument("只有相连的曲线能映射为管壁轮廓。");
    }
    return TopoDS::Wire(shape);
}

}

QPainterPath unfoldedTextPath(const cadcam::Text& text)
{
    QFont font(QString::fromStdString(text.font));
    constexpr int fontPixels = 100;
    font.setPixelSize(fontPixels);
    QPainterPath outline;
    outline.addText(QPointF(), font, QString::fromUtf8(text.utf8.c_str()));
    QTransform transform;
    transform.translate(text.insertion.x, text.insertion.y);
    transform.rotate(text.rotationDegrees);
    transform.scale(text.height / fontPixels, text.height / fontPixels);
    return transform.map(outline);
}

TubeUnfold::TubeUnfold(const TubePart& part) : length_(part.ends ? part.ends->length : 0)
{
    if (part.type != TubeType::BoxTube || part.source != TubeSource::Created ||
        !part.ends || !std::isfinite(length_) || length_ <= Tolerance || part.ends->outlines.front().IsNull()) {
        throw std::invalid_argument("二维展开暂支持软件新建的方矩管。");
    }

    std::vector<Strip> profile;
    std::size_t top = 0;
    double topZ = -std::numeric_limits<double>::infinity();
    for (BRepTools_WireExplorer it(part.ends->outlines.front()); it.More(); it.Next()) {
        const auto& edge = it.Current();
        const BRepAdaptor_Curve curve(edge);
        const auto kind = curve.GetType();
        if (kind != GeomAbs_Line && kind != GeomAbs_Circle) {
            throw std::invalid_argument("方矩管截面须由直线和圆弧组成。");
        }
        const double from = curve.FirstParameter(), to = curve.LastParameter();
        const double span = GCPnts_AbscissaPoint::Length(curve, from, to);
        if (!std::isfinite(span) || span <= Tolerance) {
            throw std::invalid_argument("管材截面包含无效的边。");
        }
        if (kind == GeomAbs_Line) {
            const auto start = curve.Value(from), end = curve.Value(to);
            if (std::abs(start.Z() - end.Z()) <= Tolerance && start.Z() > topZ) {
                topZ = start.Z();
                top = profile.size();
            }
        }
        const bool reversed = it.Orientation() == TopAbs_REVERSED;
        profile.push_back({edge, reversed ? to : from, reversed ? from : to, 0, span,
            kind == GeomAbs_Circle ? curve.Circle().Radius() : 0});
    }
    if (profile.empty() || !std::isfinite(topZ)) {
        throw std::invalid_argument("无法确定方矩管的顶面展开零线。");
    }

    const BRepAdaptor_Curve topCurve(profile[top].edge);
    if (topCurve.Value(profile[top].first).Y() < topCurve.Value(profile[top].last).Y()) {
        std::reverse(profile.begin(), profile.end());
        for (auto& strip : profile) std::swap(strip.first, strip.last);
        top = profile.size() - 1 - top;
    }
    std::rotate(profile.begin(), profile.begin() + top, profile.end());

    // Both half-strips refer to the same face and meet at the periodic seam.
    auto first = profile.front();
    const double middle = first.first + (first.last - first.first) / 2;
    first.first = middle;
    first.length /= 2;
    profile.front().last = middle;
    profile.front().length /= 2;
    std::rotate(profile.begin(), profile.begin() + 1, profile.end());
    profile.insert(profile.begin(), first);
    for (auto& strip : profile) {
        strip.start = perimeter_;
        perimeter_ += strip.length;
    }
    strips_ = std::move(profile);
}

std::vector<double> TubeUnfold::guides() const
{
    std::vector<double> boundaries;
    boundaries.reserve(strips_.size() - 1);
    for (std::size_t i = 1; i < strips_.size(); ++i) boundaries.push_back(strips_[i].start);
    return boundaries;
}

double TubeUnfold::wrap(double s) const
{
    const double v = std::fmod(s, perimeter_);
    return v < 0 ? v + perimeter_ : v;
}

const TubeUnfold::Strip& TubeUnfold::stripAt(double s) const
{
    auto end = std::upper_bound(strips_.begin(), strips_.end(), s,
        [](double value, const Strip& strip) { return value < strip.start; });
    return *std::prev(end);
}

Point TubeUnfold::map(cadcam::Point pt) const
{
    if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || pt.x < -Tolerance || pt.x > length_ + Tolerance) {
        throw std::invalid_argument("图形超出管材轴向范围。");
    }
    const double s = wrap(pt.y);
    const auto& strip = stripAt(s);
    const BRepAdaptor_Curve curve(strip.edge);
    const double ratio = std::clamp((s - strip.start) / strip.length, 0.0, 1.0);
    const auto section = curve.Value(strip.first + ratio * (strip.last - strip.first));
    return {std::clamp(pt.x, 0.0, length_), section.Y(), section.Z()};
}

bool TubeUnfold::withinPlane(double low, double high) const
{
    const double center = low + (high - low) / 2;
    const double wrapped = wrap(center);
    const auto& strip = stripAt(wrapped);
    if (strip.radius > 0) return false;
    const double offset = center - wrapped;
    const double start = (&strip == &strips_.front()) ? -strips_.back().length : strip.start;
    const double end = (&strip == &strips_.back()) ? perimeter_ + strips_.front().length : strip.start + strip.length;
    return low >= offset + start - Tolerance && high <= offset + end + Tolerance;
}

Plane TubeUnfold::tangentPlane(cadcam::Point pt) const
{
    const auto at = map(pt);
    const double s = wrap(pt.y);
    const auto& strip = stripAt(s);
    const BRepAdaptor_Curve curve(strip.edge);
    const double ratio = std::clamp((s - strip.start) / strip.length, 0.0, 1.0);
    gp_Pnt point;
    gp_Vec tangent;
    curve.D1(strip.first + ratio * (strip.last - strip.first), point, tangent);
    tangent.Normalize();
    if (strip.last < strip.first) tangent.Reverse();
    return {at, {0, tangent.Z(), -tangent.Y()}, {1, 0, 0}};
}

Primitive TubeUnfold::mapPrimitive(const cadcam::Primitive& primitive) const
{
    if (const auto* point = std::get_if<cadcam::Point>(&primitive)) return map(*point);
    if (const auto* text = std::get_if<cadcam::Text>(&primitive)) {
        const auto outline = unfoldedTextPath(*text);
        if (outline.isEmpty()) throw std::invalid_argument("文字没有可显示的字形。");
        const auto bounds = outline.boundingRect();
        validateBounds(bounds.left(), bounds.top(), bounds.right(), bounds.bottom());

        BRep_Builder builder;
        TopoDS_Compound contours;
        builder.MakeCompound(contours);
        // Use the same glyphs as the editor and flatten them at model tolerance.
        const auto scale = QTransform::fromScale(1 / ChordTolerance, 1 / ChordTolerance);
        for (const auto& polygon : outline.toSubpathPolygons(scale)) {
            BRepBuilderAPI_MakePolygon flat;
            for (const auto& point : polygon) flat.Add(gp_Pnt(point.x() * ChordTolerance, point.y() * ChordTolerance, 0));
            if (!flat.IsDone()) continue;
            const auto span = polygon.boundingRect();
            builder.Add(contours, mapWire(flat.Wire(), span.top() * ChordTolerance, span.bottom() * ChordTolerance));
        }
        if (contours.NbChildren() == 0) throw std::invalid_argument("文字没有有效的轮廓。");
        auto plane = tangentPlane(text->insertion);
        const double angle = cadcam::degreesToRadians(text->rotationDegrees);
        plane.xDirection = {std::cos(angle), -plane.normal.z * std::sin(angle), plane.normal.y * std::sin(angle)};
        return Text{plane, text->utf8, text->font, text->height, contours};
    }

    const auto flat = cadcam::makeOcctShape(primitive);
    if (flat.IsNull()) throw std::invalid_argument("图形为空或轮廓未连接。");
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(flat, bounds, false, false);
    double left, low, nearZ, right, high, farZ;
    bounds.Get(left, low, nearZ, right, high, farZ);
    validateBounds(left, low, right, high);
    if (const auto* rect = std::get_if<cadcam::Rectangle>(&primitive); rect && withinPlane(low, high)) {
        const auto [x, y] = rect->lowerLeft;
        return Rectangle{{map({x, y}), map({x + rect->width, y}),
            map({x + rect->width, y + rect->height}), map({x, y + rect->height})}};
    }
    return mapWire(asWire(flat), low, high);
}

void TubeUnfold::validateBounds(double axialMin, double circumMin, double axialMax, double circumMax) const
{
    if (axialMin < -Tolerance || axialMax > length_ + Tolerance) {
        throw std::invalid_argument("图形超出管材轴向范围。");
    }
    if (circumMax - circumMin >= perimeter_ - Tolerance) {
        throw std::invalid_argument("单个图形的周向跨度必须小于一整圈。");
    }
}

TopoDS_Wire TubeUnfold::mapWire(const TopoDS_Wire& flat, double low, double high) const
{
    if (withinPlane(low, high)) {
        const double middle = (low + high) / 2;
        const auto plane = tangentPlane({0, middle});
        const double dy = -plane.normal.z, dz = plane.normal.y;
        gp_Trsf transform;
        // X and circumferential tangent map the planar curve without resampling.
        transform.SetValues(1, 0, 0, 0,
            0, dy, -dz, plane.origin.y - middle * dy,
            0, dz, dy, plane.origin.z - middle * dz);
        return TopoDS::Wire(BRepBuilderAPI_Transform(flat, transform, true).Shape());
    }

    // shortcut: curved and cross-face contours use presentation chords, preserve surface curves before machining.
    BRepBuilderAPI_MakePolygon polygon;
    gp_Pnt previous;
    bool hasPrevious = false;
    const auto append = [&](cadcam::Point p) {
        const auto xyz = map(p);
        const gp_Pnt current(xyz.x, xyz.y, xyz.z);
        if (!hasPrevious || previous.Distance(current) > Tolerance) {
            polygon.Add(current);
            previous = current;
            hasPrevious = true;
        }
    };
    const auto appendChord = [&](cadcam::Point a, cadcam::Point b) {
        if (std::abs(a.y - b.y) <= Tolerance) {
            append(b);
            return;
        }
        const double low = std::min(a.y, b.y), high = std::max(a.y, b.y);
        std::vector<double> fractions{0, 1};
        for (const auto& strip : strips_) {
            const double boundary = low + wrap(strip.start - low);
            if (boundary > low + Tolerance && boundary < high - Tolerance) {
                fractions.push_back((boundary - a.y) / (b.y - a.y));
            }
        }
        std::sort(fractions.begin(), fractions.end());
        for (std::size_t i = 1; i < fractions.size(); ++i) {
            const double from = fractions[i - 1], to = fractions[i];
            const auto& strip = stripAt(wrap(a.y + (b.y - a.y) * (from + to) / 2));
            const double width = std::abs((b.y - a.y) * (to - from));
            const int count = strip.radius > 0 ?
                std::max(1, static_cast<int>(std::ceil(width / std::sqrt(8 * strip.radius * ChordTolerance)))) : 1;
            for (int j = 1; j <= count; ++j) {
                const double t = from + (to - from) * j / count;
                append({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)});
            }
        }
    };

    for (BRepTools_WireExplorer it(flat); it.More(); it.Next()) {
        const BRepAdaptor_Curve curve(it.Current());
        const GCPnts_QuasiUniformDeflection sampled(curve, ChordTolerance);
        if (!sampled.IsDone() || sampled.NbPoints() < 2) {
            throw std::invalid_argument("无法计算跨面的图形。");
        }
        const bool reversed = it.Orientation() == TopAbs_REVERSED;
        cadcam::Point previousFlat{};
        for (int i = 1; i <= sampled.NbPoints(); ++i) {
            const auto value = sampled.Value(reversed ? sampled.NbPoints() + 1 - i : i);
            const cadcam::Point current{value.X(), value.Y()};
            if (i == 1) append(current);
            else appendChord(previousFlat, current);
            previousFlat = current;
        }
    }
    if (!polygon.IsDone()) throw std::invalid_argument("展开图形未能映射成有效轮廓。");
    return polygon.Wire();
}

Shape TubeUnfold::mapShape(const SketchShape& shape) const
{
    if (const auto* primitive = std::get_if<cadcam::Primitive>(&shape.content)) {
        return {shape.id, mapPrimitive(*primitive), shape.operations};
    }
    std::vector<Shape> members;
    for (const auto& member : std::get<std::vector<SketchShape>>(shape.content)) {
        members.push_back(mapShape(member));
    }
    return {shape.id, std::move(members), shape.operations};
}

}
