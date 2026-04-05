#pragma once
#include <glm/glm.hpp>

namespace Marble {

  // Alias so game code writes Color instead of raw glm::vec4.
  // Components are (red, green, blue, alpha), all in [0, 1].
  using Color = glm::vec4;

  namespace Colors {
    inline const Color White       = {1.00f, 1.00f, 1.00f, 1.0f};
    inline const Color Black       = {0.00f, 0.00f, 0.00f, 1.0f};
    inline const Color Transparent = {0.00f, 0.00f, 0.00f, 0.0f};
    inline const Color Red         = {1.00f, 0.00f, 0.00f, 1.0f};
    inline const Color Green       = {0.00f, 0.80f, 0.00f, 1.0f};
    inline const Color Blue        = {0.00f, 0.40f, 1.00f, 1.0f};
    inline const Color Yellow      = {1.00f, 1.00f, 0.00f, 1.0f};
    inline const Color Cyan        = {0.00f, 1.00f, 1.00f, 1.0f};
    inline const Color Magenta     = {1.00f, 0.00f, 1.00f, 1.0f};
    inline const Color Orange      = {1.00f, 0.50f, 0.00f, 1.0f};
    inline const Color Gray        = {0.50f, 0.50f, 0.50f, 1.0f};
    inline const Color LightGray   = {0.75f, 0.75f, 0.75f, 1.0f};
    inline const Color DarkGray    = {0.25f, 0.25f, 0.25f, 1.0f};
  }

} // namespace Marble
