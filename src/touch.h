#pragma once

#include <stdint.h>

namespace touch {

bool begin();
// Rising edge of a finger press. Coordinates are ignored (head-touch substitute).
bool wasPressed();

}  // namespace touch
