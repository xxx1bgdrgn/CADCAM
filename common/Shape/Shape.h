#pragma once

#include <cstdint>
#include <variant>
#include <vector>

namespace cadcam {

using Id = std::uint64_t;

enum class CompensationSide { Inside, Outside };
struct Compensation { CompensationSide side; double distance; };
enum class LeadStyle { Line, Arc };
struct LeadIn { LeadStyle style; double length; double angleDegrees; };
struct LeadOut { LeadStyle style; double length; double angleDegrees; };
using Operation = std::variant<Compensation, LeadIn, LeadOut>;

template<class Geometry>
struct ShapeRecord
{
    Id id;
    std::variant<Geometry, std::vector<ShapeRecord>> content;
    std::vector<Operation> operations;
};

}
