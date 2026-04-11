#pragma once

namespace Marble {

  class Application;
  class Renderer2D;

  // Base class for all game logic. Override the virtual callbacks to implement
  // your game. Application calls them in this order:
  //
  //   OnStart()
  //   loop: OnUpdate(dt) → OnRender(renderer)
  //   OnStop()
  //
  // The protected App() accessor gives direct access to the running Application
  // without going through the global singleton — valid from OnStart to OnStop.
  class GameLayer {
  public:
    virtual ~GameLayer() = default;

    virtual void OnStart()                          {}
    virtual void OnUpdate(float dt)                 {}
    virtual void OnRender(Renderer2D& renderer)     {}
    // HUD pass drawn after the post-process blit, directly into the window
    // framebuffer at native screen resolution. Text and UI elements are always
    // crisp regardless of the game's render style (PixelArt / Smooth).
    //
    // The renderer is already in a BeginScene with a camera that maps
    // render-space coordinates (0..renderW, 0..renderH) to the letterbox
    // viewport — just draw, no BeginScene / EndScene needed.
    virtual void OnHudRender(Renderer2D& renderer)  {}
    virtual void OnStop()                           {}
    virtual void OnResize()                         {}

  protected:
    Application&       App()       { return *m_App; }
    const Application& App() const { return *m_App; }

  private:
    Application* m_App = nullptr;
    friend class Application; // sets m_App before OnStart, clears after OnStop
  };

} // namespace Marble
