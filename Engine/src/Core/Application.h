#pragma once
#include "Core/Window.h"
#include "GameLayer.h"
#include "Color.h"
#include "Renderer/Camera.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/PostProcessSettings.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Marble {

  class Application {
  public:
    explicit Application(const WindowSpec& spec = {});
    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    void Run(GameLayer& layer);

    // ── Window ───────────────────────────────────────────────────────
    // Full window access for runtime setters (SetVSync, SetTitle, …).
    Window&       GetWindow()       { return m_Window; }
    const Window& GetWindow() const { return m_Window; }

    // Convenience forwarding — equivalents of App().GetWindow().X()
    void SetFullscreen(bool fullscreen)                         { m_Window.SetFullscreen(fullscreen);  }
    bool IsFullscreen() const                                   { return m_Window.IsFullscreen();       }
    bool IsMinimized()  const                                   { return m_Window.IsMinimized();        }
    int  GetWidth()     const                                   { return m_Window.GetWidth();           }
    int  GetHeight()    const                                   { return m_Window.GetHeight();          }
    void SetIcon(const std::string& path)                       { m_Window.SetIcon(path);               }
    void SetIcon(const std::vector<std::string>& paths)         { m_Window.SetIcon(paths);              }

    // ── Render target ────────────────────────────────────────────────
    int  GetRenderWidth()  const { return m_RenderWidth;  }
    int  GetRenderHeight() const { return m_RenderHeight; }

    // ── Time ─────────────────────────────────────────────────────────
    float GetTime() const { return m_Time; }

    // ── Clear color ──────────────────────────────────────────────────
    // Background color cleared before each game render. Default is a dark charcoal.
    void SetClearColor(const Color& color) { m_ClearColor = color; }

    // ── Post-processing ──────────────────────────────────────────────
    PostProcessSettings& GetPostProcessSettings() { return m_PostProcessSettings; }

  private:
    struct Viewport { int X = 0, Y = 0, W = 0, H = 0; };

    void     OnResize(int width, int height);
    Viewport ComputeLetterboxViewport() const;

    // ── Window (MUST be declared first — destroyed last, after all GL objects) ─
    // C++ destroys members in reverse declaration order. Declaring Window first
    // ensures the GL context outlives Renderer2D, Framebuffer, PostProcessPass,
    // and all other OpenGL-owning objects that are declared below it.
    Window m_Window;

    // ── Render state ─────────────────────────────────────────────────
    bool m_FixedRenderSize = false;
    int  m_RenderWidth     = 0;
    int  m_RenderHeight    = 0;

    // ── Game ─────────────────────────────────────────────────────────
    GameLayer* m_Layer = nullptr;
    float      m_Time  = 0.0f;

    // ── Renderer ─────────────────────────────────────────────────────
    std::unique_ptr<Renderer2D>         m_Renderer;
    std::unique_ptr<Framebuffer>        m_Framebuffer;
    std::unique_ptr<PostProcessPass>    m_PostProcess;
    std::unique_ptr<OrthographicCamera> m_HudCamera;
    PostProcessSettings                 m_PostProcessSettings;
    Color                               m_ClearColor = { 0.1f, 0.1f, 0.15f, 1.0f };

#ifdef MARBLE_DEBUG
    // ── Perf HUD (debug builds only) ─────────────────────────────────
    void TickPerfHUD(float dt);

    static constexpr int k_PerfSamples = 90;
    float    m_FrameTimes[k_PerfSamples] = {};
    int      m_PerfHead       = 0;
    float    m_CpuUsagePct    = 0.0f;
    uint64_t m_LastCpuTotal   = 0;
    uint64_t m_LastWallTime   = 0;
    float    m_StatPollTimer  = 0.0f;
    float    m_RamUsageMB     = 0.0f;
    int      m_NumCores       = 1;
    bool     m_PerfHUDVisible = true;
#endif
  };

} // namespace Marble
