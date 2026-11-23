#pragma once

namespace cadcam {

constexpr double degreesToRadians(double degrees)
{
    constexpr double Pi = 3.14159265358979323846;
    return degrees * Pi / 180.0;
}

}
