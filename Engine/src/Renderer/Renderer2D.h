#pragma once
#include "Font.h"
#include "Texture.h"
#include "Camera.h"
#include "../Core/Color.h"
#include <glm/glm.hpp>
#include <memory>
#include <string_view>

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
    // Fast path: axis-aligned quad, no matrix. Used by all non-rotated DrawQuad overloads.
    void      SubmitQuadDirect(const glm::vec2& pos, const glm::vec2& size,
                               const Color& color, float texIndex);
    // Slow path: rotated quad, applies a pre-built transform matrix.
    void      SubmitQuad(const glm::mat4& transform, const Color& color, float texIndex);
    // Like SubmitQuad but uses per-vertex UV coordinates (for font glyph quads).
    void      SubmitGlyph(const glm::vec2& center, const glm::vec2& size,
                          const Color& color, float texIndex, const glm::vec2 uvs[4]);
    glm::mat4 BuildTransform(const glm::vec2& pos, const glm::vec2& size, float rotDeg);

    // Internal state is hidden behind Pimpl to keep this header free of
    // OpenGL types, raw arrays, and MAX_* constants.
    struct Data;
    std::unique_ptr<Data> m_Data;
  };

} // namespace Marble
