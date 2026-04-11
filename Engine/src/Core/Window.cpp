#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#include "Window.h"
#include <stb_image.h>
#include <stdexcept>
#include <vector>

// GLFW_INCLUDE_NONE prevents GLFW from including its own OpenGL headers — GLAD
// handles that in the files that actually make OpenGL calls. Window.cpp never
// makes any GL calls, so GLAD is not included here.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Marble {

  Window::Window(const WindowSpec& spec) {
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW");
    }

    // ── Detect primary monitor ────────────────────────────────────────────────
    GLFWmonitor*       primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid     = primary ? glfwGetVideoMode(primary) : nullptr;

    const int nativeW = vid ? vid->width  : 1920;
    const int nativeH = vid ? vid->height : 1080;

    // 0 == auto-detect from monitor
    const int windowW = spec.Width  ? spec.Width  : nativeW;
    const int windowH = spec.Height ? spec.Height : nativeH;

    // Windowed restore size = spec dimensions, centered.
    // Used both for F11 → windowed and as the drag-restore size when
    // un-maximizing by dragging the title bar.
    m_WindowedWidth  = windowW;
    m_WindowedHeight = windowH;
    m_WindowedX      = (nativeW - m_WindowedWidth)  / 2;
    m_WindowedY      = (nativeH - m_WindowedHeight) / 2;

    // ── Create window ─────────────────────────────────────────────────────────
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Maximize on launch only when no explicit window size was requested.
    // With an explicit size (e.g. 1280×720), the window should start at that
    // size. Without one, it fills the monitor, so maximized is the right start.
    const bool startMaximized = (spec.Width == 0 && spec.Height == 0 && !spec.Fullscreen);
    glfwWindowHint(GLFW_MAXIMIZED, startMaximized ? GLFW_TRUE : GLFW_FALSE);

    if (spec.Fullscreen && primary && vid) {
      // True fullscreen at the monitor's native video mode.
      m_Fullscreen = true;
      m_Handle = glfwCreateWindow(vid->width, vid->height, spec.Title, primary, nullptr);
    } else {
      m_Handle = glfwCreateWindow(windowW, windowH, spec.Title, nullptr, nullptr);
    }

    if (!m_Handle) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_Handle);
    glfwSwapInterval(spec.VSync ? 1 : 0);

    // ── Register callbacks ────────────────────────────────────────────────────
    glfwSetWindowUserPointer     (m_Handle, this);
    glfwSetWindowSizeCallback    (m_Handle, OnWindowSizeChanged);
    glfwSetWindowFocusCallback   (m_Handle, OnWindowFocus);
    glfwSetWindowCloseCallback   (m_Handle, OnWindowClose);
    glfwSetWindowMaximizeCallback(m_Handle, OnWindowMaximize);

    // ── DWM styling ───────────────────────────────────────────────────────────
    m_TitleBarColor = spec.TitleBarColor;
    ApplyDWMStyling();

    // ── Seed WINDOWPLACEMENT ──────────────────────────────────────────────────
    // Seeds the window's "normal" (restored) position so that dragging the title
    // bar while maximized un-maximizes to our intended windowed size, not the
    // full monitor size that GLFW_MAXIMIZED would otherwise leave as the default.
    {
      HWND hwnd = glfwGetWin32Window(m_Handle);
      WINDOWPLACEMENT wp = { sizeof(wp) };
      GetWindowPlacement(hwnd, &wp);
      wp.rcNormalPosition = {
        m_WindowedX,
        m_WindowedY,
        m_WindowedX + m_WindowedWidth,
        m_WindowedY + m_WindowedHeight
      };
      SetWindowPlacement(hwnd, &wp);
    }

    // ── Seed framebuffer dimensions ───────────────────────────────────────────
    // Use glfwGetFramebufferSize rather than spec.Width/Height because:
    // 1. Fullscreen size is determined by the video mode, not spec dimensions
    // 2. HiDPI (Retina etc.) scales the framebuffer independently of logical px
    glfwGetFramebufferSize(m_Handle, &m_Width, &m_Height);
  }

  Window::~Window() {
    glfwDestroyWindow(m_Handle);
    glfwTerminate();
  }

  // ── Per-frame ─────────────────────────────────────────────────────────────────
  void Window::PollEvents()          { glfwPollEvents(); }
  void Window::SwapBuffers()         { glfwSwapBuffers(m_Handle); }
  void Window::WaitEvents()          { glfwWaitEvents(); }
  bool Window::ShouldClose() const   { return glfwWindowShouldClose(m_Handle) != 0; }

  // ── Runtime setters ───────────────────────────────────────────────────────────
  void Window::SetVSync(bool vsync) {
    glfwSwapInterval(vsync ? 1 : 0);
  }

  void Window::SetTitle(const char* title) {
    glfwSetWindowTitle(m_Handle, title);
  }

  void Window::SetTitleBarColor(uint32_t color) {
    m_TitleBarColor = color;
    ApplyDWMStyling();
  }

  void Window::SetIcon(const std::string& path) {
    SetIcon(std::vector<std::string>{ path });
  }

  void Window::SetIcon(const std::vector<std::string>& paths) {
    std::vector<GLFWimage>      images;
    std::vector<unsigned char*> pixelBuffers;

    for (const auto& path : paths) {
      int x, y, channels;
      unsigned char* px = stbi_load(path.c_str(), &x, &y, &channels, 4);
      if (!px) {
        throw std::runtime_error("Failed to load icon: " + path);
      }
      images.push_back({ x, y, px });
      pixelBuffers.push_back(px);
    }

    glfwSetWindowIcon(m_Handle, static_cast<int>(images.size()), images.data());

    for (unsigned char* px : pixelBuffers) {
      stbi_image_free(px);
    }
  }

  // ── DWM styling ───────────────────────────────────────────────────────────────
  void Window::ApplyDWMStyling() const {
    HWND hwnd = glfwGetWin32Window(m_Handle);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    if (m_TitleBarColor != 0xFFFFFFFF) {
      // WindowSpec stores 0x00RRGGBB
      // COLORREF is       0x00BBGGRR
      COLORREF color = RGB(
        (m_TitleBarColor >> 16) & 0xFF,
        (m_TitleBarColor >>  8) & 0xFF,
         m_TitleBarColor        & 0xFF
      );
      DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
    }
  }

  // ── Fullscreen ────────────────────────────────────────────────────────────────
  void Window::SetFullscreen(bool fullscreen) {
    if (m_Fullscreen == fullscreen) return;
    m_Fullscreen = fullscreen;

    HWND hwnd = glfwGetWin32Window(m_Handle);

    if (fullscreen) {
      // ── Save windowed state ───────────────────────────────────────────────
      m_WindowedMaximized = IsZoomed(hwnd) != 0;
      m_WindowedStyle     = GetWindowLong(hwnd, GWL_STYLE);

      // Always save the restored (non-maximized) position so it returns to the
      // right size even if the window was maximized when F11 was pressed.
      WINDOWPLACEMENT wp = { sizeof(wp) };
      GetWindowPlacement(hwnd, &wp);
      m_WindowedX      = wp.rcNormalPosition.left;
      m_WindowedY      = wp.rcNormalPosition.top;
      m_WindowedWidth  = wp.rcNormalPosition.right  - wp.rcNormalPosition.left;
      m_WindowedHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

      // ── Go borderless ────────────────────────────────────────────────────
      HMONITOR    hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi   = { sizeof(mi) };
      GetMonitorInfo(hmon, &mi);

      SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
      SetWindowPos(hwnd, HWND_TOP,
        mi.rcMonitor.left,
        mi.rcMonitor.top,
        mi.rcMonitor.right  - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
      // ── Restore windowed ─────────────────────────────────────────────────
      // Apply DWM styling while the window is still WS_POPUP (no title bar).
      // DWM attributes are per-HWND and persist across style changes, so when
      // the title bar reappears it already has the correct color.
      ApplyDWMStyling();

      SetWindowLong(hwnd, GWL_STYLE, m_WindowedStyle);

      WINDOWPLACEMENT wp = { sizeof(wp) };
      GetWindowPlacement(hwnd, &wp);
      wp.showCmd          = m_WindowedMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
      wp.rcNormalPosition = { m_WindowedX, m_WindowedY,
                              m_WindowedX + m_WindowedWidth,
                              m_WindowedY + m_WindowedHeight };
      SetWindowPlacement(hwnd, &wp);

      SetForegroundWindow(hwnd);
    }
  }

  // ── GLFW callbacks ────────────────────────────────────────────────────────────
  void Window::OnWindowSizeChanged(GLFWwindow* window, int width, int height) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (width == 0 || height == 0) {
      win->m_Minimized = true;
      return;
    }
    win->m_Minimized = false;

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    win->m_Width  = fbW;
    win->m_Height = fbH;

    if (win->m_ResizeCallback) {
      win->m_ResizeCallback(fbW, fbH);
    }
  }

  void Window::OnWindowMaximize(GLFWwindow* window, int /*maximized*/) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    win->m_Minimized = false;
    win->m_Width     = fbW;
    win->m_Height    = fbH;

    if (win->m_ResizeCallback) {
      win->m_ResizeCallback(fbW, fbH);
    }
  }

  // Registered with GLFW so the window user pointer is always valid for future
  // use (e.g. pausing audio on focus loss, confirming quit). No-ops for now.
  void Window::OnWindowFocus(GLFWwindow*, int) {}
  void Window::OnWindowClose(GLFWwindow*)      {}

} // namespace Marble
