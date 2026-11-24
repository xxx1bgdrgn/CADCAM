#pragma once

#include <Quantity_Color.hxx>

#include <array>

namespace cadcam {

inline constexpr std::array<int, 3> ContourRgb{235, 235, 0};

inline Quantity_Color occtContourColor()
{
    const auto [red, green, blue] = ContourRgb;
    return {red / 255.0, green / 255.0, blue / 255.0, Quantity_TOC_sRGB};
}

}
