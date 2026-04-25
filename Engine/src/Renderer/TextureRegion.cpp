#include "TextureRegion.h"
#include <cassert>

namespace Marble {

  TextureRegion::TextureRegion(const Texture& texture)
    : Tex(&texture)
    , UVMin(0.0f, 0.0f)
    , UVMax(1.0f, 1.0f)
  {}

  TextureRegion::TextureRegion(const Texture& texture, int x, int y, int w, int h)
    : Tex(&texture)
  {
    assert(w > 0 && h > 0 && "TextureRegion: width and height must be positive");
    assert(x >= 0 && y >= 0 && "TextureRegion: origin must be non-negative");
    assert(x + w <= texture.GetWidth()  && "TextureRegion: region exceeds texture width");
    assert(y + h <= texture.GetHeight() && "TextureRegion: region exceeds texture height");
    // stbi_set_flip_vertically_on_load flips the image so that GL UV (0,0) is
    // at the bottom-left of the original image (i.e. the last row of pixels).
    //
    // For a region at (x, y, w, h) in image space (Y-down, y=0 at image top):
    //
    //   s0 = x / texW                          (left edge, unchanged)
    //   s1 = (x + w) / texW                    (right edge, unchanged)
    //   t0 = (texH - y - h) / texH             (GL bottom of region)
    //   t1 = (texH - y)     / texH             (GL top of region)
    //
    // Verification with full texture (x=0, y=0, w=texW, h=texH):
    //   t0 = (texH - 0 - texH) / texH = 0    ✓ matches s_TexCoords BL
    //   t1 = (texH - 0)        / texH = 1    ✓ matches s_TexCoords TL
    const float texW = static_cast<float>(texture.GetWidth());
    const float texH = static_cast<float>(texture.GetHeight());

    UVMin.x = static_cast<float>(x)         / texW;
    UVMax.x = static_cast<float>(x + w)     / texW;
    UVMin.y = static_cast<float>(texture.GetHeight() - y - h) / texH;
    UVMax.y = static_cast<float>(texture.GetHeight() - y)     / texH;
  }

} // namespace Marble
