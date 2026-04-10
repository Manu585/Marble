#include "SpriteSheet.h"
#include <stdexcept>

namespace Marble {

  SpriteSheet::SpriteSheet(const std::string& path, int frameW, int frameH, TextureSpec spec)
    : m_Texture(path, spec)
    , m_FrameW(frameW)
    , m_FrameH(frameH)
  {
    if (frameW <= 0 || frameH <= 0)
      throw std::invalid_argument("SpriteSheet: frame dimensions must be positive");

    m_Cols = m_Texture.GetWidth()  / frameW;
    m_Rows = m_Texture.GetHeight() / frameH;

    if (m_Cols == 0 || m_Rows == 0)
      throw std::invalid_argument(
          "SpriteSheet: frame size (" + std::to_string(frameW) + "x" +
          std::to_string(frameH) + ") is larger than the texture (" +
          std::to_string(m_Texture.GetWidth()) + "x" +
          std::to_string(m_Texture.GetHeight()) + ")");
  }

  TextureRegion SpriteSheet::GetFrame(int col, int row) const {
    return TextureRegion(m_Texture,
        col * m_FrameW,
        row * m_FrameH,
        m_FrameW,
        m_FrameH);
  }

  TextureRegion SpriteSheet::GetFrame(int index) const {
    return GetFrame(index % m_Cols, index / m_Cols);
  }

} // namespace Marble
