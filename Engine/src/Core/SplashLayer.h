#pragma once
#include "GameLayer.h"
#include "Color.h"
#include "Renderer/Texture.h"
#include "Renderer/Camera.h"
#include <string>
#include <vector>
#include <memory>

namespace Marble {

  // Displays a sequence of full-screen splash images with fade-in / hold / fade-out,
  // then transparently switches to the game layer you provide.
  //
  // Any key press or mouse button skips the entire sequence immediately.
  //
  // The next splash texture is preloaded during the current entry's Hold phase
  // so there is no hitch when advancing between entries.
  //
  // Usage:
  //   PongGame game;
  //   Marble::SplashLayer splash({
  //       { "assets/company_logo.png", 0.6f, 1.5f, 0.6f },
  //       { "assets/engine_logo.png",  0.4f, 1.2f, 0.4f },
  //   }, game);
  //   app.Run(splash);
  class SplashLayer : public GameLayer {
  public:
    struct Entry {
      std::string path;
      float       fadeIn     = 0.5f;        // seconds to fade in
      float       hold       = 1.5f;        // seconds to hold at full opacity
      float       fadeOut    = 0.5f;        // seconds to fade out
      Color       background = Colors::Black; // letterbox / border fill color
    };

    // screens  — ordered list of splash entries (at least one).
    // next     — game layer to switch to when the sequence finishes.
    //            Must outlive the SplashLayer (typically stack-allocated in main).
    SplashLayer(std::vector<Entry> screens, GameLayer& next);

    void OnStart()                       override;
    void OnUpdate(float dt)              override;
    void OnRender(Renderer2D& r)         override;
    void OnStop()                        override;
    void OnResize()                      override;

  private:
    enum class Phase { FadeIn, Hold, FadeOut };

    // Load the texture for m_Entries[index], return nullptr if path is empty.
    std::unique_ptr<Texture> LoadEntry(int index) const;

    // Advance to the next entry or trigger the game-layer switch.
    void Advance();

    // Current alpha in [0, 1] based on phase and timer.
    float Alpha() const;

    // Compute a centered "letterfit" rect (preserve texture aspect, fill render area).
    void ComputeImageRect(glm::vec2& outPos, glm::vec2& outSize) const;

    std::vector<Entry>           m_Entries;
    GameLayer&                   m_Next;

    int                          m_Index   = 0;
    Phase                        m_Phase   = Phase::FadeIn;
    float                        m_Timer   = 0.0f;

    std::unique_ptr<Texture>     m_Current;  // texture on screen right now
    std::unique_ptr<Texture>     m_Preload;  // next entry's texture, loaded during Hold

    std::unique_ptr<OrthographicCamera> m_Camera;

    float m_RW = 0.0f;
    float m_RH = 0.0f;
  };

} // namespace Marble
