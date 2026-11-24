#pragma once

#include "Features/Project/TubePart.h"

#include <TopoDS_Edge.hxx>
#include <QPainterPath>

namespace cadcam3d {

QPainterPath unfoldedTextPath(const cadcam::Text& text);

class TubeUnfold
{
public:
    explicit TubeUnfold(const TubePart& part);

    double length() const { return length_; }
    double perimeter() const { return perimeter_; }
    double firstFaceHalfWidth() const { return strips_.front().length; }
    std::vector<double> guides() const;
    Point map(cadcam::Point pt) const;
    Shape mapShape(const SketchShape& shape) const;

private:
    struct Strip
    {
        TopoDS_Edge edge;
        double first;
        double last;
        double start;
        double length;
        double radius;
    };

    double wrap(double s) const;
    const Strip& stripAt(double s) const;
    bool withinPlane(double low, double high) const;
    Plane tangentPlane(cadcam::Point pt) const;
    void validateBounds(double axialMin, double circumMin, double axialMax, double circumMax) const;
    TopoDS_Wire mapWire(const TopoDS_Wire& flat, double low, double high) const;
    Primitive mapPrimitive(const cadcam::Primitive& primitive) const;

    double length_;
    double perimeter_ = 0;
    std::vector<Strip> strips_;
};

}
