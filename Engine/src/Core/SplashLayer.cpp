#include "SplashLayer.h"
#include "Application.h"
#include "Input/Input.h"
#include <algorithm>
#include <cmath>

namespace Marble {

  SplashLayer::SplashLayer(std::vector<Entry> screens, GameLayer& next)
    : m_Entries(std::move(screens))
    , m_Next(next)
  {
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────────────

  void SplashLayer::OnStart() {
    m_RW = static_cast<float>(App().GetRenderWidth());
    m_RH = static_cast<float>(App().GetRenderHeight());

    m_Camera = std::make_unique<OrthographicCamera>(0.0f, m_RW, 0.0f, m_RH);

    m_Index  = 0;
    m_Phase  = Phase::FadeIn;
    m_Timer  = 0.0f;

    // Load the first texture immediately.
    m_Current = LoadEntry(0);

    // Pre-load the second texture now if there is one — avoids a hitch later.
    // (For a single-entry sequence this is a no-op.)
    m_Preload = LoadEntry(1);
  }

  void SplashLayer::OnStop() {
    m_Current.reset();
    m_Preload.reset();
    m_Camera.reset();
  }

  void SplashLayer::OnResize() {
    m_RW = static_cast<float>(App().GetRenderWidth());
    m_RH = static_cast<float>(App().GetRenderHeight());
    if (m_Camera)
      m_Camera->SetProjection(0.0f, m_RW, 0.0f, m_RH);
  }

  // ── Update ────────────────────────────────────────────────────────────────────

  void SplashLayer::OnUpdate(float dt) {
    // Any key or mouse button skips the whole sequence.
    if (Input::IsKeyJustPressed(Key::Space)    ||
        Input::IsKeyJustPressed(Key::Enter)    ||
        Input::IsKeyJustPressed(Key::Escape)   ||
        Input::IsMouseJustPressed(Mouse::Left) ||
        Input::IsMouseJustPressed(Mouse::Right))
    {
      App().SwitchLayer(m_Next);
      return;
    }

    m_Timer += dt;
    const Entry& e = m_Entries[m_Index];

    switch (m_Phase) {
      case Phase::FadeIn:
        if (m_Timer >= e.fadeIn) {
          m_Timer -= e.fadeIn;
          m_Phase  = Phase::Hold;
        }
        break;

      case Phase::Hold:
        // Kick off preload of the entry after next, if not already loaded.
        // (Entries 0 and 1 are handled in OnStart; here we cover 2+.)
        if (!m_Preload) {
          m_Preload = LoadEntry(m_Index + 1);
        }
        if (m_Timer >= e.hold) {
          m_Timer -= e.hold;
          m_Phase  = Phase::FadeOut;
        }
        break;

      case Phase::FadeOut:
        if (m_Timer >= e.fadeOut) {
          Advance();
        }
        break;
    }
  }

  // ── Render ────────────────────────────────────────────────────────────────────

  void SplashLayer::OnRender(Renderer2D& r) {
    if (m_Index >= static_cast<int>(m_Entries.size())) return;

    const Entry& e     = m_Entries[m_Index];
    const float  alpha = Alpha();

    r.BeginScene(*m_Camera);

    // Solid background (fills render target — background color fades with the logo).
    const Color bg = { e.background.r, e.background.g, e.background.b, alpha };
    r.DrawQuad({ m_RW * 0.5f, m_RH * 0.5f }, { m_RW, m_RH }, bg);

    // Centered image, letterfit inside the render area, faded by alpha.
    if (m_Current) {
      glm::vec2 pos, size;
      ComputeImageRect(pos, size);
      r.DrawQuad(pos, size, *m_Current, { 1.0f, 1.0f, 1.0f, alpha });
    }

    r.EndScene();
  }

  // ── Helpers ───────────────────────────────────────────────────────────────────

  std::unique_ptr<Texture> SplashLayer::LoadEntry(int index) const {
    if (index < 0 || index >= static_cast<int>(m_Entries.size())) return nullptr;
    const std::string& path = m_Entries[index].path;
    if (path.empty()) return nullptr;
    return std::make_unique<Texture>(path);
  }

  void SplashLayer::Advance() {
    m_Index++;
    if (m_Index >= static_cast<int>(m_Entries.size())) {
      // Sequence complete — hand off to the real game layer.
      App().SwitchLayer(m_Next);
      return;
    }

    // Swap the preloaded texture in; preload the one after that.
    m_Current = std::move(m_Preload);
    m_Preload = LoadEntry(m_Index + 1);

    m_Phase = Phase::FadeIn;
    m_Timer = 0.0f;
  }

  float SplashLayer::Alpha() const {
    if (m_Index >= static_cast<int>(m_Entries.size())) return 0.0f;
    const Entry& e = m_Entries[m_Index];
    switch (m_Phase) {
      case Phase::FadeIn:  return m_Timer / std::max(e.fadeIn,  0.0001f);
      case Phase::Hold:    return 1.0f;
      case Phase::FadeOut: return 1.0f - m_Timer / std::max(e.fadeOut, 0.0001f);
    }
    return 1.0f;
  }

  void SplashLayer::ComputeImageRect(glm::vec2& outPos, glm::vec2& outSize) const {
    if (!m_Current) { outPos = {}; outSize = {}; return; }

    const float texW = static_cast<float>(m_Current->GetWidth());
    const float texH = static_cast<float>(m_Current->GetHeight());

    // Scale the image to fit inside the render area while preserving its aspect ratio.
    const float scaleX = m_RW / texW;
    const float scaleY = m_RH / texH;
    const float scale  = std::min(scaleX, scaleY);

    outSize = { texW * scale, texH * scale };
    outPos  = { m_RW * 0.5f, m_RH * 0.5f }; // DrawQuad positions by center
  }

} // namespace Marble
