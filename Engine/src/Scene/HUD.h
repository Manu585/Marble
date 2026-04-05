#pragma once
#include "Renderer/Camera.h"
#include "Renderer/Renderer2D.h"

namespace Marble {

  // Screen-space rendering helper.
  //
  // Wraps a fixed orthographic camera that always maps (0,0) to the
  // bottom-left of the render target and (renderWidth, renderHeight) to
  // the top-right — independent of the world camera's position.
  //
  // Typical usage in GameLayer::OnRender:
  //
  //   m_Scene->Render(r);         // world-space pass
  //
  //   m_HUD->BeginRender(r);
  //   r.DrawQuad({10, 10}, {200, 30}, Colors::Red);   // health bar
  //   m_HUD->EndRender(r);
  //
  // Call Resize() from GameLayer::OnResize so the camera stays in sync.
  class HUD {
  public:
    HUD(int renderWidth, int renderHeight);

    // Update the screen-space projection when the render resolution changes.
    void Resize(int renderWidth, int renderHeight);

    // Begin a screen-space scene. All DrawQuad calls until EndRender use
    // pixel coordinates (origin = bottom-left of the render target).
    void BeginRender(Renderer2D& renderer) const;

    // Flush and end the HUD scene.
    void EndRender(Renderer2D& renderer) const;

  private:
    OrthographicCamera m_Camera;
  };

} // namespace Marble
