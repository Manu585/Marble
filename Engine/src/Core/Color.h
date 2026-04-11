#pragma once
#include <glm/glm.hpp>

namespace Marble {

  // Alias so game code writes Color instead of raw glm::vec4.
  // Components are (red, green, blue, alpha), all in [0, 1].
  using Color = glm::vec4;

  namespace Colors {
    inline constexpr Color White       = {1.00f, 1.00f, 1.00f, 1.0f};
    inline constexpr Color Black       = {0.00f, 0.00f, 0.00f, 1.0f};
    inline constexpr Color Transparent = {0.00f, 0.00f, 0.00f, 0.0f};
    inline constexpr Color Red         = {1.00f, 0.00f, 0.00f, 1.0f};
    inline constexpr Color Green       = {0.00f, 0.80f, 0.00f, 1.0f};
    inline constexpr Color Blue        = {0.00f, 0.40f, 1.00f, 1.0f};
    inline constexpr Color Yellow      = {1.00f, 1.00f, 0.00f, 1.0f};
    inline constexpr Color Cyan        = {0.00f, 1.00f, 1.00f, 1.0f};
    inline constexpr Color Magenta     = {1.00f, 0.00f, 1.00f, 1.0f};
    inline constexpr Color Orange      = {1.00f, 0.50f, 0.00f, 1.0f};
    inline constexpr Color Gray        = {0.50f, 0.50f, 0.50f, 1.0f};
    inline constexpr Color LightGray   = {0.75f, 0.75f, 0.75f, 1.0f};
    inline constexpr Color DarkGray    = {0.25f, 0.25f, 0.25f, 1.0f};
  }

} // namespace Marble
