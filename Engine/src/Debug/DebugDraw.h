#pragma once
#include "Physics/Collision.h"
#include "Renderer/Camera.h"
#include "Core/Color.h"
#include <glm/glm.hpp>

// DebugDraw — immediate-mode overlay for development diagnostics.
//
// All draw calls accumulate geometry each frame. Call Flush() (typically at the
// end of GameLayer::OnRender) to render everything with the world camera, then
// everything is cleared for the next frame.
//
// In Release builds (MARBLE_DEBUG not defined) every function is an inline
// no-op — the compiler eliminates all call sites with zero runtime cost.
// No #ifdef guards are needed in game code.
//
// Example usage (inside OnRender, after r.EndScene()):
//
//   Marble::DebugDraw::Rect(m_PlayerAABB, Marble::Colors::Cyan);
//   Marble::DebugDraw::Line({0,0}, {100,100}, Marble::Colors::Yellow);
//   Marble::DebugDraw::Flush(*m_Camera);

namespace Marble::DebugDraw {

#ifdef MARBLE_DEBUG

  // ── Engine lifecycle — called by Application, not game code ──────────────
  void Init();
  void Shutdown();
  void BeginFrame(); // clears the previous frame's geometry

  // ── Rendering — call Flush() after your draw calls, before EndScene ───────
  // Renders all accumulated shapes using the supplied camera and then clears.
  void Flush(const OrthographicCamera& camera);

  // ── Draw calls ────────────────────────────────────────────────────────────
  void Line  (glm::vec2 from, glm::vec2 to,   const Color& color = Colors::Cyan);
  void Rect  (glm::vec2 center, glm::vec2 size, const Color& color = Colors::Cyan); // wireframe
  void Rect  (const AABB& aabb,                 const Color& color = Colors::Cyan);
  void Circle(glm::vec2 center, float radius,   const Color& color = Colors::Cyan, int segments = 24);

  // ── Toggle ────────────────────────────────────────────────────────────────
  void SetEnabled(bool enabled);
  bool IsEnabled();

#else // Release — all no-ops, zero runtime cost

  inline void Init()       {}
  inline void Shutdown()   {}
  inline void BeginFrame() {}

  inline void Flush(const OrthographicCamera&) {}

  inline void Line  (glm::vec2, glm::vec2, const Color& = {})          {}
  inline void Rect  (glm::vec2, glm::vec2, const Color& = {})          {}
  inline void Rect  (const AABB&,           const Color& = {})          {}
  inline void Circle(glm::vec2, float,      const Color& = {}, int = 24){}

  inline void SetEnabled(bool) {}
  inline bool IsEnabled()      { return false; }

#endif // MARBLE_DEBUG

} // namespace Marble::DebugDraw
