#include "DebugDraw.h"

#ifdef MARBLE_DEBUG

#include "Renderer/Shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <memory>
#include <vector>

namespace Marble::DebugDraw {

  // ── Vertex format ─────────────────────────────────────────────────────────────

  struct LineVertex {
    glm::vec2 Position;
    glm::vec4 Color;
  };

  // ── Embedded GLSL ────────────────────────────────────────────────────────────
  // Kept as string literals so no extra asset files are needed for engine-internal
  // shaders. These shaders are only compiled in Debug builds.

  static constexpr const char* k_VertSrc = R"GLSL(
#version 460 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main() {
    v_Color     = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 0.0, 1.0);
}
)GLSL";

  static constexpr const char* k_FragSrc = R"GLSL(
#version 460 core
in  vec4 v_Color;
out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)GLSL";

  // ── State ─────────────────────────────────────────────────────────────────────

  struct DebugState {
    uint32_t VAO = 0;
    uint32_t VBO = 0;
    std::unique_ptr<Shader> LineShader;
    std::vector<LineVertex> Vertices; // two vertices per line segment
    bool Enabled = true;
  };

  static std::unique_ptr<DebugState> g_State;

  // ── Lifecycle ─────────────────────────────────────────────────────────────────

  void Init() {
    g_State = std::make_unique<DebugState>();

    glGenVertexArrays(1, &g_State->VAO);
    glGenBuffers     (1, &g_State->VBO);

    glBindVertexArray(g_State->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_State->VBO);

    // Allocate a generous initial buffer; it will grow via glBufferData as needed.
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // a_Position (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, Position)));
    glEnableVertexAttribArray(0);

    // a_Color (vec4)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, Color)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    g_State->LineShader = std::make_unique<Shader>(Shader::FromSource, k_VertSrc, k_FragSrc);
  }

  void Shutdown() {
    if (!g_State) return;
    glDeleteVertexArrays(1, &g_State->VAO);
    glDeleteBuffers     (1, &g_State->VBO);
    g_State.reset();
  }

  void BeginFrame() {
    if (g_State) g_State->Vertices.clear();
  }

  // ── Rendering ─────────────────────────────────────────────────────────────────

  void Flush(const OrthographicCamera& camera) {
    if (!g_State || !g_State->Enabled) return;
    if (g_State->Vertices.empty()) return;

    const auto byteSize =
        static_cast<GLsizeiptr>(g_State->Vertices.size() * sizeof(LineVertex));

    glBindBuffer(GL_ARRAY_BUFFER, g_State->VBO);
    glBufferData(GL_ARRAY_BUFFER, byteSize, g_State->Vertices.data(), GL_DYNAMIC_DRAW);

    g_State->LineShader->Bind();
    g_State->LineShader->SetMat4("u_ViewProjection", camera.GetViewProjection());

    glBindVertexArray(g_State->VAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(g_State->Vertices.size()));
    glBindVertexArray(0);
  }

  // ── Internal push helper ──────────────────────────────────────────────────────

  static void PushLine(glm::vec2 from, glm::vec2 to, const Color& color) {
    if (!g_State || !g_State->Enabled) return;
    g_State->Vertices.push_back({ from, color });
    g_State->Vertices.push_back({ to,   color });
  }

  // ── Draw calls ───────────────────────────────────────────────────────────────

  void Line(glm::vec2 from, glm::vec2 to, const Color& color) {
    PushLine(from, to, color);
  }

  void Rect(glm::vec2 center, glm::vec2 size, const Color& color) {
    const float hw = size.x * 0.5f;
    const float hh = size.y * 0.5f;
    const glm::vec2 bl = { center.x - hw, center.y - hh };
    const glm::vec2 br = { center.x + hw, center.y - hh };
    const glm::vec2 tr = { center.x + hw, center.y + hh };
    const glm::vec2 tl = { center.x - hw, center.y + hh };

    PushLine(bl, br, color); // bottom
    PushLine(br, tr, color); // right
    PushLine(tr, tl, color); // top
    PushLine(tl, bl, color); // left
  }

  void Rect(const AABB& aabb, const Color& color) {
    Rect(aabb.Center, aabb.Size(), color);
  }

  void Circle(glm::vec2 center, float radius, const Color& color, int segments) {
    if (segments < 3) segments = 3;
    const float step = 2.0f * 3.14159265358979f / static_cast<float>(segments);
    glm::vec2 prev = { center.x + radius, center.y };
    for (int i = 1; i <= segments; ++i) {
      const float angle = static_cast<float>(i) * step;
      const glm::vec2 curr = {
          center.x + radius * std::cos(angle),
          center.y + radius * std::sin(angle)
      };
      PushLine(prev, curr, color);
      prev = curr;
    }
  }

  // ── Toggle ────────────────────────────────────────────────────────────────────

  void SetEnabled(bool enabled) { if (g_State) g_State->Enabled = enabled; }
  bool IsEnabled()              { return g_State && g_State->Enabled; }

} // namespace Marble::DebugDraw

#endif // MARBLE_DEBUG
