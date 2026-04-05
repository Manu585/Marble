#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace Marble {

  // Loads a TrueType font and bakes it to an OpenGL atlas texture at a given
  // pixel height. Used with Renderer2D::DrawText.
  //
  // The atlas is stored as GL_R8 with swizzle masks (1,1,1,coverage), so the
  // standard batch shader multiplies it by the tint color to produce colored text.
  class Font {
  public:
    // path:        path to a .ttf file (relative to the executable)
    // pixelHeight: native bake size in pixels — scale in DrawText adjusts from here
    explicit Font(const std::string& path, float pixelHeight = 32.0f);
    ~Font();

    Font(const Font&)            = delete;
    Font& operator=(const Font&) = delete;

    // Per-glyph render data, pre-computed at load time.
    // All measurements are in pixels at the font's native pixelHeight.
    struct Glyph {
      float xOff;    // X offset from cursor to left edge of the quad
      float yOff;    // Y offset from baseline to TOP edge of the quad (Y-up, usually positive)
      float Width;   // quad width
      float Height;  // quad height (positive)
      float s0, t0;  // UV at top-left of the glyph in the atlas
      float s1, t1;  // UV at bottom-right
      float Advance; // horizontal cursor advance
    };

    // Returns nullptr for characters outside ASCII 32–126.
    const Glyph* GetGlyph(char c) const;

    float    GetPixelHeight() const { return m_PixelHeight; }
    uint32_t GetAtlasID()     const { return m_AtlasTexture; }

  private:
    static constexpr int FIRST_CHAR = 32;   // ' '
    static constexpr int LAST_CHAR  = 126;  // '~'
    static constexpr int CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1;
    static constexpr int ATLAS_W    = 512;
    static constexpr int ATLAS_H    = 512;

    float    m_PixelHeight  = 0.0f;
    uint32_t m_AtlasTexture = 0;
    std::array<Glyph, CHAR_COUNT> m_Glyphs{};
  };

} // namespace Marble
