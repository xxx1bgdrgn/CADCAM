#pragma once

#include "Features/Project/TubePart.h"

#include <string>

namespace cadcam3d {

// The first workspace accepts one solid per STEP file.
TubePart importStepTube(const std::string& utf8Path);

}
