#pragma once
#include "Font.h"
#include "Texture.h"
#include "TextureRegion.h"
#include "Camera.h"
#include "../Core/Color.h"
#include <glm/glm.hpp>
#include <memory>
#include <string_view>

// Win32 headers define DrawText as a macro (→ DrawTextA/DrawTextW).
// Undefine it so Renderer2D::DrawText is not silently renamed at call sites.
#ifdef DrawText
#  undef DrawText
#endif

namespace Marble {

  // Batched 2D renderer. Application owns one instance and passes it by reference
  // to GameLayer::OnRender — game code never constructs this directly.
  //
  // Usage per frame:
  //   renderer.BeginScene(camera);
  //   renderer.DrawQuad(...);
  //   renderer.DrawText(...);
  //   renderer.EndScene();   // flushes the batch
  class Renderer2D {
  public:
    Renderer2D();
    ~Renderer2D();

    Renderer2D(const Renderer2D&)            = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    void BeginScene(const OrthographicCamera& camera);
    void EndScene();
    void Flush();

    // ── Quad draw calls ───────────────────────────────────────────
    // Position is the center of the quad. Size is full width/height.
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Color& color);
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Texture& texture, const Color& tint = Colors::White);
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotationDeg, const Color& color);
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotationDeg, const Texture& texture, const Color& tint = Colors::White);

    // TextureRegion overloads — draw a sub-region of a texture (sprite sheet frame, atlas cell).
    // The region carries its own Texture reference and pre-computed GL UVs.
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, const TextureRegion& region, const Color& tint = Colors::White);
    void DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotationDeg, const TextureRegion& region, const Color& tint = Colors::White);

    // ── Text draw calls ───────────────────────────────────────────
    // pos:   baseline start position (Y-up, pixel coordinates)
    // scale: multiplier on the font's native pixel height
    void DrawText(const Font& font, std::string_view text,
                  const glm::vec2& pos, float scale = 1.0f,
                  const Color& color = Colors::White);

    // Measures the width of a string in pixels at a given scale (no draw).
    float MeasureText(const Font& font, std::string_view text, float scale = 1.0f) const;

  private:
    void      StartBatch();
    float     ResolveTextureSlot(uint32_t texID);
    // Fast path: axis-aligned quad, no matrix. Standard full-texture UVs.
    void      SubmitQuadDirect(const glm::vec2& pos, const glm::vec2& size,
                               const Color& color, float texIndex);
    // Fast path: axis-aligned quad with custom per-vertex UVs (glyphs, TextureRegion).
    void      SubmitQuadDirectUV(const glm::vec2& pos, const glm::vec2& size,
                                 const Color& color, float texIndex, const glm::vec2 uvs[4]);
    // Slow path: rotated quad, standard full-texture UVs.
    void      SubmitQuad(const glm::mat4& transform, const Color& color, float texIndex);
    // Slow path: rotated quad with custom per-vertex UVs (rotated TextureRegion).
    void      SubmitQuadUV(const glm::mat4& transform, const Color& color,
                           float texIndex, const glm::vec2 uvs[4]);
    glm::mat4 BuildTransform(const glm::vec2& pos, const glm::vec2& size, float rotDeg);

    // Internal state is hidden behind Pimpl to keep this header free of
    // OpenGL types, raw arrays, and MAX_* constants.
    struct Data;
    std::unique_ptr<Data> m_Data;
  };

} // namespace Marble
