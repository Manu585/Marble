#pragma once
#include "Texture.h"
#include <glm/glm.hpp>

namespace Marble {

  // A rectangular sub-region of a Texture, with pre-computed GL UV coordinates.
  //
  // Pixel coordinates are specified in image space: X right, Y down, origin at
  // the top-left corner of the image — matching every common image editor and
  // sprite-sheet layout tool.
  //
  // UV conversion accounts for the vertical flip applied by stbi at load time
  // (stbi_set_flip_vertically_on_load), so the resulting UVs map correctly in
  // OpenGL without any additional transform.
  //
  // Usage:
  //   TextureRegion full(myTex);                    // entire texture
  //   TextureRegion frame(myTex, 0, 0, 16, 16);     // 16×16 pixel region at (0,0)
  //   r.DrawQuad(pos, size, frame);
  struct TextureRegion {
    // Covers the entire texture.
    explicit TextureRegion(const Texture& texture);

    // Pixel rectangle in image space (Y=0 at the top of the image, Y-down).
    //   x, y  — top-left corner of the region in the source image
    //   w, h  — width and height of the region in pixels
    TextureRegion(const Texture& texture, int x, int y, int w, int h);

    const Texture* Tex = nullptr;

    // Pre-computed normalized GL UV coordinates after vertical-flip adjustment.
    //   UVMin = (s0, t0) — bottom-left of the region in GL UV space
    //   UVMax = (s1, t1) — top-right  of the region in GL UV space
    glm::vec2 UVMin{ 0.0f, 0.0f };
    glm::vec2 UVMax{ 1.0f, 1.0f };
  };

} // namespace Marble
