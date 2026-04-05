#include "Font.h"

// stb_truetype declarations — implementation is compiled in vendor/stb/stb_truetype.cpp
#define STBTT_DEF extern
#include <stb_truetype.h>

#include <glad/glad.h>

#include <fstream>
#include <stdexcept>
#include <vector>

namespace Marble {

  Font::Font(const std::string& path, float pixelHeight) : m_PixelHeight(pixelHeight) {
    // ── Load .ttf file ────────────────────────────────────────────────────────
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      throw std::runtime_error("Failed to open font file: " + path);

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fontData(fileSize);
    file.read(reinterpret_cast<char*>(fontData.data()), static_cast<std::streamsize>(fileSize));

    // ── Bake glyph bitmaps to an atlas ────────────────────────────────────────
    std::vector<uint8_t> atlasData(ATLAS_W * ATLAS_H, 0);
    std::array<stbtt_bakedchar, CHAR_COUNT> bakedChars{};

    const int rowsUsed = stbtt_BakeFontBitmap(
        fontData.data(), 0, pixelHeight,
        atlasData.data(), ATLAS_W, ATLAS_H,
        FIRST_CHAR, CHAR_COUNT, bakedChars.data()
    );
    if (rowsUsed <= 0)
      throw std::runtime_error(
          "Font atlas too small for '" + path + "' at " + std::to_string(pixelHeight) + "px. "
          "Increase ATLAS_W/H or reduce pixelHeight.");

    // ── Build Glyph table ─────────────────────────────────────────────────────
    // Convert stbtt_bakedchar (Y-down screen space) to our Y-up Glyph format.
    //
    // stbtt conventions (Y-down):
    //   xoff  = signed X offset from cursor to left edge of the quad
    //   yoff  = signed Y offset from cursor to top of the quad (negative = above cursor)
    //   x0/y0 = top-left pixel of the glyph in the atlas
    //   x1/y1 = bottom-right pixel of the glyph in the atlas
    //   xadvance = horizontal cursor advance
    //
    // In our Y-up engine, "above the baseline" is larger Y.
    // So glyph top Y = baselineY + (-yoff) = baselineY - yoff.
    for (int i = 0; i < CHAR_COUNT; i++) {
      const stbtt_bakedchar& bc = bakedChars[i];
      Glyph& g = m_Glyphs[i];

      g.xOff    = bc.xoff;
      g.yOff    = -bc.yoff;                           // flip Y-down → Y-up
      g.Width   = static_cast<float>(bc.x1 - bc.x0);
      g.Height  = static_cast<float>(bc.y1 - bc.y0);
      g.s0      = bc.x0 / static_cast<float>(ATLAS_W);
      g.t0      = bc.y0 / static_cast<float>(ATLAS_H); // top of glyph in atlas
      g.s1      = bc.x1 / static_cast<float>(ATLAS_W);
      g.t1      = bc.y1 / static_cast<float>(ATLAS_H); // bottom of glyph in atlas
      g.Advance = bc.xadvance;
    }

    // ── Upload atlas as GL_R8 with swizzle ────────────────────────────────────
    // Atlas data is 8-bit coverage (0=transparent, 255=opaque).
    // Swizzle: (R→1, G→1, B→1, A→R) so sampling returns (1,1,1,coverage).
    // The batch shader then multiplies by the tint: final = tint * (1,1,1,coverage).
    // This lets any tint color be used as the text color without a special shader.
    glGenTextures(1, &m_AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R,  GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G,  GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B,  GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A,  GL_RED);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  Font::~Font() {
    glDeleteTextures(1, &m_AtlasTexture);
  }

  const Font::Glyph* Font::GetGlyph(char c) const {
    if (c < FIRST_CHAR || c > LAST_CHAR) return nullptr;
    return &m_Glyphs[static_cast<unsigned char>(c) - FIRST_CHAR];
  }

} // namespace Marble
