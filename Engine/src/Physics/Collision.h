#pragma once
#include <cmath>
#include <glm/glm.hpp>

namespace Marble {

  // Axis-Aligned Bounding Box defined by its center and half-extents.
  // All measurements are in world units (pixels for a pixel-art game).
  struct AABB {
    glm::vec2 Center;
    glm::vec2 HalfSize; // half-width, half-height

    // ── Factory ──────────────────────────────────────────────────────────────
    // Build an AABB from a center position and full size.
    static AABB FromCenter(glm::vec2 center, glm::vec2 size) {
      return { center, size * 0.5f };
    }

    // ── Queries ───────────────────────────────────────────────────────────────
    // True if this box overlaps another (touching counts as overlap).
    bool Overlaps(const AABB& other) const {
      return std::abs(Center.x - other.Center.x) <= HalfSize.x + other.HalfSize.x
          && std::abs(Center.y - other.Center.y) <= HalfSize.y + other.HalfSize.y;
    }

    glm::vec2 Min()  const { return Center - HalfSize; }
    glm::vec2 Max()  const { return Center + HalfSize; }
    glm::vec2 Size() const { return HalfSize * 2.0f;   }
  };

} // namespace Marble
