#pragma once

#include "Shape/Primitives2D.h"

class gp_Pnt;
class TopoDS_Shape;

namespace cadcam {

gp_Pnt toOcctPoint(Point point);
TopoDS_Shape makeOcctShape(const Primitive& primitive);

}
