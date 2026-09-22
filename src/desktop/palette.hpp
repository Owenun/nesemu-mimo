#pragma once

#include "nesemu/types.hpp"

#include <array>
#include <cstdint>

namespace nesemu_desktop {

// NES master palette as RGBA8888 (alpha 255).
const std::array<std::uint32_t, 64>& nes_palette_rgba();

}  // namespace nesemu_desktop
