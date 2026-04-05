#include "HUD.h"

namespace Marble {

  HUD::HUD(int renderWidth, int renderHeight)
    : m_Camera(0.0f, static_cast<float>(renderWidth),
               0.0f, static_cast<float>(renderHeight))
  {}

  void HUD::Resize(int renderWidth, int renderHeight) {
    m_Camera.SetProjection(0.0f, static_cast<float>(renderWidth),
                           0.0f, static_cast<float>(renderHeight));
  }

  void HUD::BeginRender(Renderer2D& renderer) const {
    renderer.BeginScene(m_Camera);
  }

  void HUD::EndRender(Renderer2D& renderer) const {
    renderer.EndScene();
  }

} // namespace Marble
