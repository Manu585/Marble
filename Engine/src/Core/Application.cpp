#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#include "Application.h"
#include "Input/Input.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <stb_image.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

#ifdef _DEBUG
#  include <cstdio>
#endif

namespace Marble {

  Application::Application(const WindowSpec& spec) {

#ifdef _DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    std::printf("[Debug] Console attached\n");
#endif

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

    // Render resolution is fixed whenever the user supplies any dimension.
    // Dynamic resize only happens when everything is left at 0.
    m_FixedRenderSize = (spec.RenderWidth != 0 || spec.RenderHeight != 0 || spec.Width != 0 || spec.Height != 0);
    m_RenderWidth     = spec.RenderWidth  ? spec.RenderWidth  : windowW;
    m_RenderHeight    = spec.RenderHeight ? spec.RenderHeight : windowH;

    // Windowed restore size = spec dimensions, centered.
    // This is used both for F11 -> windowed and as the drag-restore size when
    // un-maximizing by dragging the title bar.
    m_WindowedWidth  = windowW;
    m_WindowedHeight = windowH;
    m_WindowedX      = (nativeW - m_WindowedWidth)  / 2;
    m_WindowedY      = (nativeH - m_WindowedHeight) / 2;

    // ── Create window ─────────────────────────────────────────────────────────
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    if (spec.Fullscreen && primary && vid) {
      // True fullscreen at the monitor's native video mode, no border,
      // no decoration, exclusive ownership of the display.
      m_Fullscreen = true;
      m_Window = glfwCreateWindow(vid->width, vid->height, spec.Title, primary, nullptr);
    } else {
      m_Window = glfwCreateWindow(windowW, windowH, spec.Title, nullptr, nullptr);
    }

    if (!m_Window) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(spec.VSync ? 1 : 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      throw std::runtime_error("Failed to initialize GLAD");
    }

    // OpenGL Callbacks for window actions
    glfwSetWindowUserPointer     (m_Window, this);
    glfwSetWindowSizeCallback    (m_Window, OnWindowSizeChanged);
    glfwSetWindowFocusCallback   (m_Window, OnWindowFocus);
    glfwSetWindowCloseCallback   (m_Window, OnWindowClose);
    glfwSetWindowMaximizeCallback(m_Window, OnWindowMaximize);

    m_TitleBarColor = spec.TitleBarColor;
    ApplyDWMStyling();

    // Seed the window's "normal" (restored) position so that dragging the title
    // bar while maximized un-maximizes to our intended windowed size, not the
    // full monitor size that GLFW_MAXIMIZED would otherwise leave as the default.
    {
      HWND hwnd = glfwGetWin32Window(m_Window);
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    stbi_set_flip_vertically_on_load(1);

    // Set window handle for Input class
    Input::SetWindowHandle(m_Window);

    m_Renderer    = std::make_unique<Renderer2D>();
    m_Framebuffer = std::make_unique<Framebuffer>(m_RenderWidth, m_RenderHeight, spec.Style == RenderStyle::Smooth ? TextureFilter::Linear : TextureFilter::Nearest);
    m_PostProcess = std::make_unique<PostProcessPass>();

    // Sync m_Width/Height with the real framebuffer size.
    // glfwGetFramebufferSize is used rather than spec dimensions because:
    // 1. Fullscreen size is set by the video mode, not spec.Width/Height
    // 2. HiDPI (Retina etc.) scales the framebuffer independently of logical px
    int fbW;
    int fbH;
    glfwGetFramebufferSize(m_Window, &fbW, &fbH);
    OnResize(fbW, fbH);
  }

  Application::~Application() {
    // Renderer, Framebuffer, PostProcess destroyed in reverse order by unique_ptr
    glfwDestroyWindow(m_Window);
    glfwTerminate();
#ifdef _DEBUG
    FreeConsole();
#endif
  }

  void Application::Run(GameLayer& layer) {
    m_Layer     = &layer;
    layer.m_App = this;

    layer.OnStart();

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(m_Window)) {
      glfwPollEvents();

      int fbW;
      int fbH;
      glfwGetFramebufferSize(m_Window, &fbW, &fbH);
      if (fbW != m_Width || fbH != m_Height) {
        OnResize(fbW, fbH);
      }

      // PAUSE when minimized
      if (m_Minimized) {
        glfwWaitEvents();
        continue;
      }

      const float now       = static_cast<float>(glfwGetTime());
      const float deltaTime = now - lastTime;
      m_Time               += deltaTime;
      lastTime              = now;

      Input::BeginFrame();
      layer.OnUpdate(deltaTime);

      // ── render game into fixed-resolution framebuffer ────────────
      m_Framebuffer->Bind();
      glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      layer.OnRender(*m_Renderer);
      m_Framebuffer->Unbind();

      // ── letterbox blit ───────────────────────────────────────────
      // Clear the window to black first (the bars), then restrict glViewport
      // to the aspect-correct rect so the post-process quad fills only that area.
      glViewport(0, 0, m_Width, m_Height);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      const Viewport vp = ComputeLetterboxViewport();
      glViewport(vp.X, vp.Y, vp.W, vp.H);
      m_PostProcess->Apply(*m_Framebuffer, m_PostProcessSettings, m_Time);

      glfwSwapBuffers(m_Window);
    }

    layer.OnStop();
    layer.m_App = nullptr;
    m_Layer     = nullptr;
  }

  // ── Resize ───────────────────────────────────────────────────────────────────
  void Application::OnResize(int width, int height) {
    if (width == 0 || height == 0) return; // minimized or mid-transition, skip

    m_Width  = width;
    m_Height = height;

    // When no fixed render resolution was requested, grow the framebuffer to
    // match the window so the content always fills the entire client area.
    if (!m_FixedRenderSize && m_Framebuffer) {
      m_RenderWidth  = width;
      m_RenderHeight = height;
      m_Framebuffer->Resize(width, height);
    }

    if (m_Layer) {
      m_Layer->OnResize();
    }
  }

  Application::Viewport Application::ComputeLetterboxViewport() const {
    const float targetAspect = static_cast<float>(m_RenderWidth) / static_cast<float>(m_RenderHeight);
    const float windowAspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

    Viewport vp{ 0, 0, m_Width, m_Height }; // X, Y, Width, Height
    if (windowAspect > targetAspect) {
      // Window is wider than target  -> black bars left and right
      vp.W = static_cast<int>(m_Height * targetAspect);
      vp.X = (m_Width - vp.W) / 2;
    } else {
      // Window is taller than target -> black bars top and bottom
      vp.H = static_cast<int>(m_Width / targetAspect);
      vp.Y = (m_Height - vp.H) / 2;
    }
    return vp;
  }

  // ── GLFW callbacks ───────────────────────────────────────────────────────────
  void Application::OnWindowSizeChanged(GLFWwindow* window, int width, int height) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    if (width == 0 || height == 0) {
      app->m_Minimized = true;
      return;
    }
    app->m_Minimized = false;

    int fbW;
    int fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    app->OnResize(fbW, fbH);
  }

  void Application::OnWindowMaximize(GLFWwindow* window, int /*maximized*/) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    int fbW;
    int fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) return;

    app->m_Minimized = false;
    app->OnResize(fbW, fbH);
  }

  void Application::OnWindowFocus(GLFWwindow*, int) {}
  void Application::OnWindowClose(GLFWwindow*)      {}

  // ── Icon ─────────────────────────────────────────────────────────────────────
  void Application::SetIcon(const std::string& path) {
    SetIcon(std::vector<std::string>{ path });
  }

  void Application::SetIcon(const std::vector<std::string>& paths) {
    std::vector<GLFWimage>      images;
    std::vector<unsigned char*> pixelBuffers;

    for (const auto& path : paths) {
      int x;
      int y;
      int channels;
      unsigned char* px = stbi_load(path.c_str(), &x, &y, &channels, 4);
      if (!px) {
        throw std::runtime_error("Failed to load icon: " + path);
      }
      images.push_back({ x, y, px });
      pixelBuffers.push_back(px);
    }

    glfwSetWindowIcon(m_Window, static_cast<int>(images.size()), images.data());

    for (unsigned char* px : pixelBuffers) {
      stbi_image_free(px);
    }
  }

  // ── DWM styling ──────────────────────────────────────────────────────────────
  void Application::ApplyDWMStyling() const {
    HWND hwnd = glfwGetWin32Window(m_Window);

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

  // ── Fullscreen ───────────────────────────────────────────────────────────────
  void Application::SetFullscreen(bool fullscreen) {
    if (m_Fullscreen == fullscreen) return;
    m_Fullscreen = fullscreen;

    HWND hwnd = glfwGetWin32Window(m_Window);

    if (fullscreen) {
      // ── Save windowed state ───────────────────────────────────────────────
      m_WindowedMaximized = IsZoomed(hwnd) != 0;
      m_WindowedStyle     = GetWindowLong(hwnd, GWL_STYLE);

      // Always save the restored (non-maximized) position so it returns to the
      // right size even if the window was maximized when F11 was pressed
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

} // namespace Marble
