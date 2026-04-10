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
    // Screen-space pass drawn after OnRender, inside the same FBO.
    // The renderer is already in a BeginScene with a pixel-coordinate camera
    // (origin = bottom-left, extents = render resolution). Just draw — no
    // BeginScene / EndScene needed. Resize is handled automatically.
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
