#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow; // opaque GLFW type — forward declaration avoids leaking GLFW headers

namespace Marble {

  // Controls how the render framebuffer is scaled to the window.
  // PixelArt — GL_NEAREST: crisp pixel-perfect upscaling (default)
  // Smooth   — GL_LINEAR:  bilinear interpolation, for high-res / non-pixel-art games
  enum class RenderStyle { PixelArt, Smooth };

  struct WindowSpec {
    const char* Title         = "";               // required — set via marble_configure_executable()
    int         Width         = 0;                // 0 = primary monitor width
    int         Height        = 0;                // 0 = primary monitor height
    bool        VSync         = false;
    bool        Fullscreen    = false;
    int         RenderWidth   = 0;                // 0 = match window size
    int         RenderHeight  = 0;                // 0 = match window size
    uint32_t    TitleBarColor = 0x00111111;       // 0x00RRGGBB, Windows 11+ only
    RenderStyle Style         = RenderStyle::PixelArt;
  };

  // Owns the OS window, GL context, and all GLFW/Win32 windowing state.
  // Application holds a Window as its first member so the context outlives
  // all GL objects (members are destroyed in reverse declaration order).
  class Window {
  public:
    // Fires with framebuffer dimensions whenever the window is resized.
    using ResizeCallback = std::function<void(int w, int h)>;

    explicit Window(const WindowSpec& spec);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    // ── Per-frame ────────────────────────────────────────────────────
    void PollEvents();
    void SwapBuffers();
    void WaitEvents();   // blocks until at least one event is queued (use while minimized)
    bool ShouldClose() const;

    // ── Runtime setters ──────────────────────────────────────────────
    void SetFullscreen    (bool fullscreen);
    void SetVSync         (bool vsync);
    void SetTitle         (const char* title);
    void SetIcon          (const std::string& path);
    void SetIcon          (const std::vector<std::string>& paths); // multi-size (16, 32, 64 …)
    void SetTitleBarColor (uint32_t color);  // 0x00RRGGBB, Windows 11+ only

    // ── State ────────────────────────────────────────────────────────
    int  GetWidth()     const { return m_Width;      }
    int  GetHeight()    const { return m_Height;     }
    bool IsMinimized()  const { return m_Minimized;  }
    bool IsFullscreen() const { return m_Fullscreen; }

    // Raw GLFW handle — use sparingly (Input, ImGui backend init).
    GLFWwindow* GetHandle() const { return m_Handle; }

    // Called on every resize with the new framebuffer dimensions.
    void SetResizeCallback(ResizeCallback callback) { m_ResizeCallback = std::move(callback); }

  private:
    void ApplyDWMStyling() const;

    // GLFW event callbacks — retrieve Window* via glfwGetWindowUserPointer
    static void OnWindowSizeChanged(GLFWwindow*, int w, int h);
    static void OnWindowFocus      (GLFWwindow*, int focused);
    static void OnWindowClose      (GLFWwindow*);
    static void OnWindowMaximize   (GLFWwindow*, int maximized);

    GLFWwindow* m_Handle            = nullptr;
    int         m_Width             = 0;
    int         m_Height            = 0;
    bool        m_Minimized         = false;
    bool        m_Fullscreen        = false;
    bool        m_WindowedMaximized = false;
    int32_t     m_WindowedStyle     = 0;  // Win32 GWL_STYLE at last windowed state
    int         m_WindowedX         = 0;
    int         m_WindowedY         = 0;
    int         m_WindowedWidth     = 0;
    int         m_WindowedHeight    = 0;
    uint32_t    m_TitleBarColor     = 0xFFFFFFFF;
    ResizeCallback m_ResizeCallback;
  };

} // namespace Marble
