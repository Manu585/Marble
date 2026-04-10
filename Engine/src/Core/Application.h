#pragma once
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

struct GLFWwindow;

namespace Marble {

  // Controls how the render framebuffer is scaled to the window.
  // PixelArt — GL_NEAREST: crisp pixel-perfect upscaling (default)
  // Smooth   — GL_LINEAR:  bilinear interpolation, for high-res / non-pixel-art games
  enum class RenderStyle { PixelArt, Smooth };

  struct WindowSpec {
    const char* Title        = "Marble Engine";
    int         Width        = 0;                // 0 = primary monitor width
    int         Height       = 0;                // 0 = primary monitor height
    bool        VSync        = true;
    bool        Fullscreen   = false;
    int         RenderWidth  = 0;
    int         RenderHeight = 0;
    uint32_t    TitleBarColor = 0x00111111;      // Title bar accent color (0x00RRGGBB). Windows 11+ only
    RenderStyle Style        = RenderStyle::PixelArt;
  };

  class Application {
  public:
    explicit Application(const WindowSpec& spec = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run(GameLayer& layer);

    // ── Window ───────────────────────────────────────────────────────
    void SetFullscreen(bool fullscreen);
    void SetIcon(const std::string& path);
    void SetIcon(const std::vector<std::string>& paths); // multi-size (16, 32, 64 …)

    int  GetWidth()     const { return m_Width;     }
    int  GetHeight()    const { return m_Height;    }
    bool IsMinimized()  const { return m_Minimized; }
    bool IsFullscreen() const { return m_Fullscreen;}

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
    void     ApplyDWMStyling() const;
    Viewport ComputeLetterboxViewport() const;

    // ── GLFW callbacks ───────────────────────────────────────────────
    static void OnWindowSizeChanged(GLFWwindow*, int w, int h);
    static void OnWindowFocus      (GLFWwindow*, int focused);
    static void OnWindowClose      (GLFWwindow*);
    static void OnWindowMaximize   (GLFWwindow*, int maximized);

    // ── Window ───────────────────────────────────────────────────────
    GLFWwindow* m_Window            = nullptr;
    int         m_Width             = 0;
    int         m_Height            = 0;
    bool        m_Minimized         = false;
    bool        m_Fullscreen        = false;
    bool        m_FixedRenderSize   = false;
    bool        m_WindowedMaximized = false;
    int32_t     m_WindowedStyle     = 0; // stores Win32 GWL_STYLE (LONG, always 32-bit on MSVC)
    int         m_WindowedX         = 0;
    int         m_WindowedY         = 0;
    int         m_WindowedWidth     = 0;
    int         m_WindowedHeight    = 0;
    int         m_RenderWidth       = 0;
    int         m_RenderHeight      = 0;
    uint32_t    m_TitleBarColor     = 0xFFFFFFFF;

    // ── Game ─────────────────────────────────────────────────────────
    GameLayer* m_Layer = nullptr;
    float      m_Time  = 0.0f;

    // ── Renderer ─────────────────────────────────────────────────────
    std::unique_ptr<Renderer2D>      m_Renderer;
    std::unique_ptr<Framebuffer>     m_Framebuffer;
    std::unique_ptr<PostProcessPass> m_PostProcess;
    std::unique_ptr<OrthographicCamera> m_HudCamera;   // screen-space, auto-managed
    PostProcessSettings              m_PostProcessSettings;
    Color                            m_ClearColor = { 0.1f, 0.1f, 0.15f, 1.0f };
  };

} // namespace Marble
