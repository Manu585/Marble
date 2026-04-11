#include "Renderer2D.h"
#include "Shader.h"
#include "TextureRegion.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace Marble {

  // ── Embedded batch shaders ───────────────────────────────────────────────────
  // Batch shaders are engine-level infrastructure: they handle multi-texture
  // batching and SDF font rendering — nothing game-specific. Embedding them
  // means the engine is fully self-contained and does not depend on game assets.

  static constexpr const char* k_BatchVertSrc = R"GLSL(
#version 460 core
layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;

uniform mat4 u_ViewProjection;

out vec4  v_Color;
out vec2  v_TexCoord;
out float v_TexIndex;

void main() {
    v_Color     = a_Color;
    v_TexCoord  = a_TexCoord;
    v_TexIndex  = a_TexIndex;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)GLSL";

  static constexpr const char* k_BatchFragSrc = R"GLSL(
#version 460 core
in vec4  v_Color;
in vec2  v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Textures[16];

// Per-slot flag: 1 = this slot holds an SDF font atlas, 0 = regular texture.
// Set by Renderer2D::DrawText for the resolved font atlas slot each batch.
uniform int u_SDFSlots[16];

out vec4 FragColor;

void main() {
    int  index   = int(v_TexIndex);
    vec4 sampled = texture(u_Textures[index], v_TexCoord);

    if (u_SDFSlots[index] != 0) {
        // ── SDF font path ─────────────────────────────────────────────────────
        // The font atlas stores a Signed Distance Field in the red channel,
        // remapped to alpha via GL swizzle: sampled = (1, 1, 1, sdf_value).
        // sdf_value is normalized to [0, 1]:
        //   > 0.502  →  inside the glyph  (onedge = 128, 128/255 ≈ 0.502)
        //   ≈ 0.502  →  exactly on the edge
        //   < 0.502  →  outside the glyph
        //
        // smoothstep reconstructs a sharp, anti-aliased edge at any scale or
        // sub-pixel position — unlike a plain coverage bitmap which blurs.
        float alpha = smoothstep(0.45, 0.55, sampled.a);
        FragColor   = vec4(v_Color.rgb, v_Color.a * alpha);
    } else {
        // ── Regular sprite / quad path ────────────────────────────────────────
        FragColor = sampled * v_Color;
    }
}
)GLSL";

  // ── Batch limits ─────────────────────────────────────────────────────────────
  static constexpr uint32_t MAX_QUADS         = 10'000;
  static constexpr uint32_t MAX_VERTICES      = MAX_QUADS * 4;
  static constexpr uint32_t MAX_INDICES       = MAX_QUADS * 6;
  static constexpr uint32_t MAX_TEXTURE_SLOTS = 16;

  // ── Per-vertex layout ────────────────────────────────────────────────────────
  struct QuadVertex {
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    float     TexIndex; // 0 = white 1×1 texture (flat color), N = sampler slot N
  };

  // Unit quad corners in model space, centered on origin.
  // Const, shared across all Renderer2D instances.
  static const glm::vec4 s_QuadCorners[4] = {
    { -0.5f, -0.5f, 0.0f, 1.0f },
    {  0.5f, -0.5f, 0.0f, 1.0f },
    {  0.5f,  0.5f, 0.0f, 1.0f },
    { -0.5f,  0.5f, 0.0f, 1.0f },
  };

  static constexpr glm::vec2 s_TexCoords[4] = {
    { 0.0f, 0.0f }, { 1.0f, 0.0f },
    { 1.0f, 1.0f }, { 0.0f, 1.0f },
  };

  // ── Pimpl data ───────────────────────────────────────────────────────────────
  struct Renderer2D::Data {
    uint32_t VAO          = 0;
    uint32_t VBO          = 0;
    uint32_t IBO          = 0;
    uint32_t WhiteTexture = 0;

    std::unique_ptr<Shader>        BatchShader;
    std::unique_ptr<QuadVertex[]>  VertexBuffer; // CPU staging buffer
    QuadVertex*                    VertexPtr = nullptr;

    uint32_t QuadIndexCount   = 0;
    uint32_t TextureSlotIndex = 1; // slot 0 reserved for WhiteTexture

    std::array<uint32_t, MAX_TEXTURE_SLOTS> TextureSlots{};
    std::array<int,      MAX_TEXTURE_SLOTS> SDFSlots{};  // 1 = SDF font atlas, 0 = regular texture
    glm::mat4 ViewProjection{1.0f};
  };

  // ── Constructor / Destructor (replaces Init / Shutdown) ──────────────────────
  Renderer2D::Renderer2D() : m_Data(std::make_unique<Data>()) {
    auto& d = *m_Data;

    // ── VAO / VBO ────────────────────────────────────────────────
    glGenVertexArrays(1, &d.VAO);
    glGenBuffers     (1, &d.VBO);
    glGenBuffers     (1, &d.IBO);

    glBindVertexArray(d.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, d.VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(QuadVertex), nullptr, GL_DYNAMIC_DRAW);

    const uint32_t stride = sizeof(QuadVertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(QuadVertex, Position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(QuadVertex, Color));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(QuadVertex, TexCoord));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(QuadVertex, TexIndex));
    glEnableVertexAttribArray(3);

    // ── Index buffer (static, built once) ────────────────────────
    auto indices = std::make_unique<uint32_t[]>(MAX_INDICES);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < MAX_INDICES; i += 6) {
      indices[i + 0] = offset + 0;
      indices[i + 1] = offset + 1;
      indices[i + 2] = offset + 2;
      indices[i + 3] = offset + 2;
      indices[i + 4] = offset + 3;
      indices[i + 5] = offset + 0;
      offset += 4;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(uint32_t), indices.get(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // ── CPU staging buffer ────────────────────────────────────────
    d.VertexBuffer = std::make_unique<QuadVertex[]>(MAX_VERTICES);

    // ── White 1×1 texture for flat-color quads ───────────────────
    // Using white means the shader needs no branch: color quads just sample white.
    glGenTextures(1, &d.WhiteTexture);
    glBindTexture(GL_TEXTURE_2D, d.WhiteTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    constexpr uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);

    d.TextureSlots[0] = d.WhiteTexture;

    // ── Batch shader ─────────────────────────────────────────────
    d.BatchShader = std::make_unique<Shader>(Shader::FromSource, k_BatchVertSrc, k_BatchFragSrc);
    d.BatchShader->Bind();
    int samplers[MAX_TEXTURE_SLOTS];
    for (int i = 0; i < MAX_TEXTURE_SLOTS; i++) samplers[i] = i;
    d.BatchShader->SetIntArray("u_Textures", samplers, MAX_TEXTURE_SLOTS);
  }

  Renderer2D::~Renderer2D() {
    auto& d = *m_Data;
    glDeleteVertexArrays(1, &d.VAO);
    glDeleteBuffers     (1, &d.VBO);
    glDeleteBuffers     (1, &d.IBO);
    glDeleteTextures    (1, &d.WhiteTexture);
    // VertexBuffer (unique_ptr<QuadVertex[]>) frees itself
  }

  // ── Per-frame ────────────────────────────────────────────────────────────────
  void Renderer2D::BeginScene(const OrthographicCamera& camera) {
    m_Data->ViewProjection = camera.GetViewProjection();
    StartBatch();
  }

  void Renderer2D::StartBatch() {
    auto& d = *m_Data;
    d.QuadIndexCount   = 0;
    d.VertexPtr        = d.VertexBuffer.get();
    d.TextureSlotIndex = 1;
    d.SDFSlots.fill(0);
  }

  void Renderer2D::Flush() {
    auto& d = *m_Data;
    if (d.QuadIndexCount == 0) return;

    // Upload only the filled portion of the buffer
    const auto dataSize = static_cast<uint32_t>(
      reinterpret_cast<const uint8_t*>(d.VertexPtr) -
      reinterpret_cast<const uint8_t*>(d.VertexBuffer.get())
    );

    glBindBuffer(GL_ARRAY_BUFFER, d.VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, d.VertexBuffer.get());

    for (uint32_t i = 0; i < d.TextureSlotIndex; i++) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, d.TextureSlots[i]);
    }

    d.BatchShader->Bind();
    d.BatchShader->SetMat4     ("u_ViewProjection", d.ViewProjection);
    d.BatchShader->SetIntArray ("u_SDFSlots",       d.SDFSlots.data(), MAX_TEXTURE_SLOTS);

    glBindVertexArray(d.VAO);
    glDrawElements(GL_TRIANGLES, d.QuadIndexCount, GL_UNSIGNED_INT, nullptr);
  }

  void Renderer2D::EndScene() { Flush(); }

  // ── Quad submission helpers ──────────────────────────────────────────────────

  // Fast path: axis-aligned quad, no rotation matrix needed.
  // Computes corner positions with 4 additions — the common case for the batch renderer.
  void Renderer2D::SubmitQuadDirect(const glm::vec2& pos, const glm::vec2& size,
                                    const Color& color, float texIndex) {
    auto& d = *m_Data;
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;

    // Matches s_QuadCorners layout: BL, BR, TR, TL
    const glm::vec3 corners[4] = {
      { pos.x - hw, pos.y - hh, 0.0f },
      { pos.x + hw, pos.y - hh, 0.0f },
      { pos.x + hw, pos.y + hh, 0.0f },
      { pos.x - hw, pos.y + hh, 0.0f },
    };

    for (int i = 0; i < 4; i++) {
      d.VertexPtr->Position = corners[i];
      d.VertexPtr->Color    = color;
      d.VertexPtr->TexCoord = s_TexCoords[i];
      d.VertexPtr->TexIndex = texIndex;
      d.VertexPtr++;
    }
    d.QuadIndexCount += 6;
  }

  // Slow path: rotated quad, standard full-texture UVs.
  void Renderer2D::SubmitQuad(const glm::mat4& transform, const Color& color, float texIndex) {
    auto& d = *m_Data;
    for (int i = 0; i < 4; i++) {
      d.VertexPtr->Position = transform * s_QuadCorners[i];
      d.VertexPtr->Color    = color;
      d.VertexPtr->TexCoord = s_TexCoords[i];
      d.VertexPtr->TexIndex = texIndex;
      d.VertexPtr++;
    }
    d.QuadIndexCount += 6;
  }

  // Slow path: rotated quad with custom per-vertex UVs (used for rotated TextureRegion).
  void Renderer2D::SubmitQuadUV(const glm::mat4& transform, const Color& color,
                                 float texIndex, const glm::vec2 uvs[4]) {
    auto& d = *m_Data;
    for (int i = 0; i < 4; i++) {
      d.VertexPtr->Position = transform * s_QuadCorners[i];
      d.VertexPtr->Color    = color;
      d.VertexPtr->TexCoord = uvs[i];
      d.VertexPtr->TexIndex = texIndex;
      d.VertexPtr++;
    }
    d.QuadIndexCount += 6;
  }

  float Renderer2D::ResolveTextureSlot(uint32_t texID) {
    auto& d = *m_Data;
    for (uint32_t i = 1; i < d.TextureSlotIndex; i++) {
      if (d.TextureSlots[i] == texID) return static_cast<float>(i);
    }
    if (d.TextureSlotIndex >= MAX_TEXTURE_SLOTS) { Flush(); StartBatch(); }
    const float index = static_cast<float>(d.TextureSlotIndex);
    d.TextureSlots[d.TextureSlotIndex++] = texID;
    return index;
  }

  glm::mat4 Renderer2D::BuildTransform(const glm::vec2& pos, const glm::vec2& size, float rotDeg) {
    // Build the 2D TRS matrix (T * R * S) directly — no identity matrices, no multiplications.
    // Column-major layout (GLM convention):
    //   col 0:  ( cos*sx,  sin*sx, 0, 0 )
    //   col 1:  (-sin*sy,  cos*sy, 0, 0 )
    //   col 2:  (      0,       0, 1, 0 )
    //   col 3:  (     tx,      ty, 0, 1 )
    const float rad = glm::radians(rotDeg);
    const float c = std::cos(rad), s = std::sin(rad);
    return {
      glm::vec4( c * size.x,  s * size.x, 0.0f, 0.0f),
      glm::vec4(-s * size.y,  c * size.y, 0.0f, 0.0f),
      glm::vec4( 0.0f,        0.0f,       1.0f, 0.0f),
      glm::vec4( pos.x,       pos.y,      0.0f, 1.0f),
    };
  }

  // ── DrawQuad overloads ───────────────────────────────────────────────────────
  // Non-rotated overloads take the direct (no-matrix) fast path.
  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Color& color) {
    SubmitQuadDirect(pos, size, color, 0.0f);
  }

  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Texture& texture, const Color& tint) {
    SubmitQuadDirect(pos, size, tint, ResolveTextureSlot(texture.GetID()));
  }

  // Rotated overloads go through the matrix path.
  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotDeg, const Color& color) {
    SubmitQuad(BuildTransform(pos, size, rotDeg), color, 0.0f);
  }

  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotDeg, const Texture& texture, const Color& tint) {
    SubmitQuad(BuildTransform(pos, size, rotDeg), tint, ResolveTextureSlot(texture.GetID()));
  }

  // ── TextureRegion overloads ──────────────────────────────────────────────────

  // Build the 4 per-vertex UVs for a TextureRegion.
  // Layout matches s_QuadCorners: BL, BR, TR, TL.
  static void BuildRegionUVs(const TextureRegion& region, glm::vec2 uvs[4]) {
    uvs[0] = { region.UVMin.x, region.UVMin.y }; // BL
    uvs[1] = { region.UVMax.x, region.UVMin.y }; // BR
    uvs[2] = { region.UVMax.x, region.UVMax.y }; // TR
    uvs[3] = { region.UVMin.x, region.UVMax.y }; // TL
  }

  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size,
                             const TextureRegion& region, const Color& tint) {
    glm::vec2 uvs[4];
    BuildRegionUVs(region, uvs);
    SubmitQuadDirectUV(pos, size, tint, ResolveTextureSlot(region.Tex->GetID()), uvs);
  }

  void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, float rotDeg,
                             const TextureRegion& region, const Color& tint) {
    glm::vec2 uvs[4];
    BuildRegionUVs(region, uvs);
    SubmitQuadUV(BuildTransform(pos, size, rotDeg), tint,
                 ResolveTextureSlot(region.Tex->GetID()), uvs);
  }

  // Fast path: axis-aligned quad with custom per-vertex UVs.
  // Used for font glyphs and TextureRegion quads (which are never axis-misaligned).
  void Renderer2D::SubmitQuadDirectUV(const glm::vec2& center, const glm::vec2& size,
                                      const Color& color, float texIndex, const glm::vec2 uvs[4]) {
    auto& d = *m_Data;
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;

    // Matches s_QuadCorners layout: BL, BR, TR, TL
    const glm::vec3 corners[4] = {
      { center.x - hw, center.y - hh, 0.0f },
      { center.x + hw, center.y - hh, 0.0f },
      { center.x + hw, center.y + hh, 0.0f },
      { center.x - hw, center.y + hh, 0.0f },
    };

    for (int i = 0; i < 4; i++) {
      d.VertexPtr->Position = corners[i];
      d.VertexPtr->Color    = color;
      d.VertexPtr->TexCoord = uvs[i];
      d.VertexPtr->TexIndex = texIndex;
      d.VertexPtr++;
    }
    d.QuadIndexCount += 6;
  }

  // ── Text rendering ────────────────────────────────────────────────────────────
  void Renderer2D::DrawText(const Font& font, std::string_view text,
                            const glm::vec2& pos, float scale, const Color& color) {
    const float texSlot = ResolveTextureSlot(font.GetAtlasID());
    // Mark this texture slot as an SDF atlas so the batch shader applies
    // smoothstep edge reconstruction instead of raw alpha sampling.
    m_Data->SDFSlots[static_cast<int>(texSlot)] = 1;
    float cursorX = pos.x;

    for (char c : text) {
      const Font::Glyph* g = font.GetGlyph(c);
      if (!g) {
        // Unknown character — advance by a fixed amount
        cursorX += font.GetPixelHeight() * 0.5f * scale;
        continue;
      }

      // Quad corners in world space (Y-up):
      //   top    = baseline + yOff (yOff is already in Y-up)
      //   bottom = top - height
      const float x0 = cursorX + g->xOff   * scale;
      const float y1 = pos.y   + g->yOff   * scale; // top
      const float x1 = x0     + g->Width   * scale;
      const float y0 = y1     - g->Height  * scale; // bottom

      const glm::vec2 center = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f };
      const glm::vec2 sz     = { x1 - x0,           y1 - y0           };

      // UV mapping — atlas Y=0 is image-top, which in GL is the first row we uploaded.
      // t0 = top of glyph in atlas  → maps to screen top  (corner[2]/[3], y = y1)
      // t1 = bottom of glyph atlas  → maps to screen bottom (corner[0]/[1], y = y0)
      //
      // s_QuadCorners layout:  [0]=BL [1]=BR [2]=TR [3]=TL
      const glm::vec2 uvs[4] = {
        { g->s0, g->t1 }, // BL → atlas bottom
        { g->s1, g->t1 }, // BR
        { g->s1, g->t0 }, // TR → atlas top
        { g->s0, g->t0 }, // TL
      };

      // Skip invisible glyphs (e.g. space) — still advance the cursor
      if (sz.x > 0.0f && sz.y > 0.0f)
        SubmitQuadDirectUV(center, sz, color, texSlot, uvs);
      cursorX += g->Advance * scale;
    }
  }

  float Renderer2D::MeasureText(const Font& font, std::string_view text, float scale) const {
    float width = 0.0f;
    for (char c : text) {
      const Font::Glyph* g = font.GetGlyph(c);
      width += g ? g->Advance * scale : font.GetPixelHeight() * 0.5f * scale;
    }
    return width;
  }

} // namespace Marble
