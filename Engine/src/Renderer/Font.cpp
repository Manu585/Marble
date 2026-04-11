#include "Font.h"

#define STBTT_DEF extern
#include <stb_truetype.h>

#include <glad/glad.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace Marble {

  Font::Font(const std::string& path, float pixelHeight) : m_PixelHeight(pixelHeight) {
    // ── Load .ttf file ────────────────────────────────────────────────────────
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) throw std::runtime_error("Failed to open font file: " + path);

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fontData(fileSize);
    file.read(reinterpret_cast<char*>(fontData.data()), static_cast<std::streamsize>(fileSize));

    // ── Init stbtt ────────────────────────────────────────────────────────────
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0))
      throw std::runtime_error("Failed to parse font file: " + path);

    const float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);

    // ── SDF baking parameters ─────────────────────────────────────────────────
    //
    // Instead of a plain coverage bitmap, we bake a Signed Distance Field (SDF):
    // each texel stores the normalized distance to the nearest glyph edge.
    //
    //   SDF_ONEDGE  — byte value that maps to "exactly on the edge" (≈ 0.5 normalized).
    //   SDF_PADDING — extra pixels around every glyph so the distance field extends
    //                 beyond the outline, enabling clean anti-aliasing at any scale.
    //   SDF_DIST_SCALE — SDF units per pixel of distance. Setting it to
    //                    ONEDGE/PADDING means the field saturates to 0 or 255
    //                    at exactly the padding boundary.
    //
    // The fragment shader reconstructs a sharp anti-aliased edge with smoothstep,
    // which looks correct at any scale or sub-pixel position (unlike a coverage bitmap).
    static constexpr int     SDF_PADDING    = 6;
    static constexpr uint8_t SDF_ONEDGE     = 128;                                    // ≈ 0.502 normalized
    static constexpr float   SDF_DIST_SCALE = static_cast<float>(SDF_ONEDGE) / SDF_PADDING;

    // ── Shelf-pack SDF glyphs into the atlas ──────────────────────────────────
    // Simple left-to-right, top-to-bottom shelf packer: advance shelfX per glyph,
    // wrap to a new shelf when the current row is full.
    std::vector<uint8_t> atlasData(ATLAS_W * ATLAS_H, 0);
    int shelfX = 0, shelfY = 0, shelfH = 0;

    for (int i = 0; i < CHAR_COUNT; i++) {
      const int codepoint = FIRST_CHAR + i;
      const int glyphIdx  = stbtt_FindGlyphIndex(&fontInfo, codepoint);

      if (glyphIdx == 0) {
        // Codepoint not in this font — leave glyph zeroed, DrawText skips invisible quads.
        m_Glyphs[i].Advance = pixelHeight * 0.5f; // reasonable fallback advance
        continue;
      }

      // Horizontal advance (cursor step) from the font's own metrics.
      int advW, lsb;
      stbtt_GetGlyphHMetrics(&fontInfo, glyphIdx, &advW, &lsb);
      m_Glyphs[i].Advance = advW * scale;

      // Rasterize the SDF for this glyph. Returns null for glyphs with no visible
      // outline (e.g., space, tab), in which case we only needed the advance above.
      int w = 0, h = 0, xoff = 0, yoff = 0;
      unsigned char* sdf = stbtt_GetGlyphSDF(
          &fontInfo, scale, glyphIdx,
          SDF_PADDING, SDF_ONEDGE, SDF_DIST_SCALE,
          &w, &h, &xoff, &yoff);

      if (!sdf || w <= 0 || h <= 0) {
        if (sdf) stbtt_FreeSDF(sdf, nullptr);
        continue; // invisible glyph — advance is set, no quad needed
      }

      // Wrap to next shelf when the current row is full.
      if (shelfX + w > ATLAS_W) {
        shelfY += shelfH;
        shelfX  = 0;
        shelfH  = 0;
      }

      if (shelfY + h > ATLAS_H) {
        stbtt_FreeSDF(sdf, nullptr);
        throw std::runtime_error(
            "Font atlas too small for '" + path + "' at " + std::to_string(pixelHeight) + "px. "
            "Increase ATLAS_W/H or reduce pixelHeight.");
      }

      // Copy SDF rows into the atlas.
      for (int row = 0; row < h; ++row)
        std::memcpy(&atlasData[(shelfY + row) * ATLAS_W + shelfX], &sdf[row * w], w);

      // Build the Glyph entry.
      // stbtt xoff/yoff are Y-down offsets from the cursor to the top-left of the
      // SDF bitmap (which is SDF_PADDING pixels larger than the bare glyph bbox).
      // Flip yoff → Y-up so DrawText can use it directly.
      Glyph& g = m_Glyphs[i];
      g.xOff   = static_cast<float>(xoff);
      g.yOff   = static_cast<float>(-yoff);              // Y-down → Y-up
      g.Width  = static_cast<float>(w);
      g.Height = static_cast<float>(h);
      g.s0     = static_cast<float>(shelfX)     / static_cast<float>(ATLAS_W);
      g.t0     = static_cast<float>(shelfY)     / static_cast<float>(ATLAS_H);
      g.s1     = static_cast<float>(shelfX + w) / static_cast<float>(ATLAS_W);
      g.t1     = static_cast<float>(shelfY + h) / static_cast<float>(ATLAS_H);

      shelfX += w;
      shelfH  = std::max(shelfH, h);

      stbtt_FreeSDF(sdf, nullptr);
    }

    // ── Upload atlas as GL_R8 with swizzle ────────────────────────────────────
    // Same layout as before: (R→1, G→1, B→1, A→R).
    // Sampling returns (1,1,1,sdf_value). The batch shader reads sdf_value from .a
    // and applies smoothstep to reconstruct a sharp, anti-aliased edge.
    glGenTextures(1, &m_AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
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
