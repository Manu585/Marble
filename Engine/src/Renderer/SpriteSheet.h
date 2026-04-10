#pragma once
#include "Texture.h"
#include "TextureRegion.h"
#include <string>

namespace Marble {

  // A texture split into a uniform grid of same-sized frames.
  // Frames are indexed left-to-right, top-to-bottom (row-major).
  //
  //   frame 0  = column 0, row 0  (top-left)
  //   frame 1  = column 1, row 0
  //   ...
  //   frame C  = column 0, row 1  (first frame of second row)
  //
  // Use GetFrame() to obtain a TextureRegion for a specific frame, then
  // pass it to Renderer2D::DrawQuad or store it in an Animator::Clip.
  class SpriteSheet {
  public:
    // path:    image file path (relative to the executable)
    // frameW:  width  of a single frame in pixels
    // frameH:  height of a single frame in pixels
    // spec:    texture filtering (default: Nearest, correct for pixel art)
    SpriteSheet(const std::string& path, int frameW, int frameH, TextureSpec spec = {});

    // Get a frame by column and row (both 0-indexed).
    TextureRegion GetFrame(int col, int row) const;

    // Get a frame by linear (row-major) index.
    // Equivalent to GetFrame(index % cols, index / cols).
    TextureRegion GetFrame(int index) const;

    int            GetFrameWidth()  const { return m_FrameW; }
    int            GetFrameHeight() const { return m_FrameH; }
    int            GetColumns()     const { return m_Cols;   }
    int            GetRows()        const { return m_Rows;   }
    const Texture& GetTexture()     const { return m_Texture;}

  private:
    Texture m_Texture;
    int     m_FrameW = 0;
    int     m_FrameH = 0;
    int     m_Cols   = 0;
    int     m_Rows   = 0;
  };

} // namespace Marble
