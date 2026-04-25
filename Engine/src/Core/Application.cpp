#include <windows.h>

#include "Application.h"
#include "Input/Input.h"
#include "Audio/Audio.h"
#include "Debug/DebugDraw.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>   // glfwGetTime(), glfwGetProcAddress
#include <stb_image.h>    // stbi_set_flip_vertically_on_load
#include <algorithm>
#include <stdexcept>

#ifdef _DEBUG
#  include <cstdio>
#endif

#ifdef MARBLE_DEBUG
#  include <imgui.h>
#  include <imgui_impl_glfw.h>
#  include <imgui_impl_opengl3.h>
#  include <psapi.h>
#endif

namespace Marble {

  Application::Application(const WindowSpec& spec)
    : m_Window(spec)
  {
#ifdef _DEBUG
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    std::printf("[Debug] Console attached\n");
#endif

    // Window constructor already called glfwMakeContextCurrent — context is live.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      throw std::runtime_error("Failed to initialize GLAD");
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // stb_image textures are loaded bottom-up (OpenGL convention). Icons loaded
    // later via SetIcon use stbi_load too but GLFW doesn't care about orientation.
    stbi_set_flip_vertically_on_load(1);

    Input::SetWindowHandle(m_Window.GetHandle());

    Audio::Init();
    DebugDraw::Init();

#ifdef MARBLE_DEBUG
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags  |= ImGuiConfigFlags_NoMouseCursorChange; // don't override the game cursor
    io.IniFilename   = nullptr;                               // don't write imgui.ini
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window.GetHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_NumCores = std::max(1, static_cast<int>(si.dwNumberOfProcessors));
#endif

    // Render resolution is fixed whenever the user supplies any dimension.
    // Dynamic resize only happens when everything is left at 0.
    m_FixedRenderSize = (spec.RenderWidth != 0 || spec.RenderHeight != 0 ||
                         spec.Width       != 0 || spec.Height      != 0);
    m_RenderWidth     = spec.RenderWidth  ? spec.RenderWidth  : m_Window.GetWidth();
    m_RenderHeight    = spec.RenderHeight ? spec.RenderHeight : m_Window.GetHeight();

    m_Renderer    = std::make_unique<Renderer2D>();
    m_Framebuffer = std::make_unique<Framebuffer>(m_RenderWidth, m_RenderHeight, spec.FramebufferFilter);
    m_PostProcess = std::make_unique<PostProcessPass>();
    m_HudCamera   = std::make_unique<OrthographicCamera>(
                      0.0f, static_cast<float>(m_RenderWidth),
                      0.0f, static_cast<float>(m_RenderHeight));

    // Wire up resize notifications — Window fires this callback (with framebuffer
    // dimensions) whenever the OS resizes the window.
    m_Window.SetResizeCallback([this](int w, int h) { OnResize(w, h); });
  }

  Application::~Application() {
    // Destroy ImGui and DebugDraw while the GL context is still alive.
    // The unique_ptr<Renderer2D/Framebuffer/…> members are destroyed after this
    // body runs (in reverse declaration order), but before m_Window — which is
    // declared first and therefore destroyed last, keeping the context live
    // for all GL object teardown.
#ifdef MARBLE_DEBUG
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    DebugDraw::Shutdown();
    Audio::Shutdown();

#ifdef _DEBUG
    FreeConsole();
#endif
  }

  void Application::SwitchLayer(GameLayer& next) {
    m_PendingLayer = &next;
  }

  void Application::TickFrame(float dt) {
    DebugDraw::BeginFrame();
    Input::BeginFrame();

#ifdef MARBLE_DEBUG
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (Input::IsKeyJustPressed(Key::F3)) {
      m_PerfHUDVisible = !m_PerfHUDVisible;
    }
    TickPerfHUD(dt);
#endif

    m_Layer->OnUpdate(dt);

    // ── render game into fixed-resolution framebuffer ────────────────────
    m_Framebuffer->Bind();
    glClearColor(m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_Layer->OnRender(*m_Renderer);
    m_Framebuffer->Unbind();

    // ── letterbox blit ────────────────────────────────────────────────────
    // Clear the window to black first (the bars), then restrict glViewport
    // to the aspect-correct rect so the post-process quad fills only that area.
    glViewport(0, 0, m_Window.GetWidth(), m_Window.GetHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const Viewport vp = ComputeLetterboxViewport();
    glViewport(vp.X, vp.Y, vp.W, vp.H);
    m_PostProcess->Apply(*m_Framebuffer, m_PostProcessSettings, m_Time);

    // ── HUD pass — window framebuffer, letterbox viewport ─────────────────
    // Drawn after the post-process blit so text/UI is always at native screen
    // resolution — crisp regardless of game FBO scale or GL_NEAREST upscaling.
    m_Renderer->BeginScene(*m_HudCamera);
    m_Layer->OnHudRender(*m_Renderer);
    m_Renderer->EndScene();

#ifdef MARBLE_DEBUG
    // ImGui renders directly into the window framebuffer, overlaying on top
    // of the final post-processed frame.
    glViewport(0, 0, m_Window.GetWidth(), m_Window.GetHeight());
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

    m_Window.SwapBuffers();
  }

  void Application::Run(GameLayer& layer) {
    m_Layer     = &layer;
    layer.m_App = this;
    layer.OnStart();

    float lastTime = static_cast<float>(glfwGetTime());

    // During a title-bar drag, Windows enters a modal loop that swallows
    // glfwPollEvents(). The subclassed WndProc fires WM_TIMER inside that
    // modal loop — this callback keeps the game ticking at full frame rate.
    m_Window.SetMoveTickCallback([this, &lastTime]() {
      const float now = static_cast<float>(glfwGetTime());
      const float dt  = std::min(now - lastTime, 0.05f);
      m_Time   += dt;
      lastTime  = now;
      TickFrame(dt);
    });

    while (!m_Window.ShouldClose()) {
      m_Window.PollEvents();

      // ── Layer switch (queued by SwitchLayer during the previous frame) ───
      if (m_PendingLayer) {
        m_Layer->OnStop();
        m_Layer->m_App = nullptr;
        m_Layer        = m_PendingLayer;
        m_PendingLayer = nullptr;
        m_Layer->m_App = this;
        m_Layer->OnStart();
        lastTime = static_cast<float>(glfwGetTime()); // reset dt — no spike on switch
      }

      // PAUSE when minimized — WaitEvents blocks until an event wakes us up,
      // avoiding a busy-spin that would peg a CPU core while invisible.
      if (m_Window.IsMinimized()) {
        m_Window.WaitEvents();
        continue;
      }

      const float now = static_cast<float>(glfwGetTime());
      // Cap deltaTime to 50 ms (≈20 FPS minimum). Without this, any stall —
      // OS sleep, debugger break, un-minimize — produces a massive spike that
      // explodes physics and gameplay state on the next frame.
      const float deltaTime = std::min(now - lastTime, 0.05f);
      m_Time    += deltaTime;
      lastTime   = now;

      TickFrame(deltaTime);
    }

    m_Window.SetMoveTickCallback(nullptr);
    m_Layer->OnStop();
    m_Layer->m_App = nullptr;
    m_Layer        = nullptr;
  }

  // ── Resize ───────────────────────────────────────────────────────────────────
  void Application::OnResize(int width, int height) {
    if (width == 0 || height == 0) return; // minimized or mid-transition, skip

    // When no fixed render resolution was requested, grow the framebuffer to
    // match the window so the content always fills the entire client area.
    if (!m_FixedRenderSize && m_Framebuffer) {
      m_RenderWidth  = width;
      m_RenderHeight = height;
      m_Framebuffer->Resize(width, height);
    }

    if (m_HudCamera) {
      m_HudCamera->SetProjection(0.0f, static_cast<float>(m_RenderWidth),
                                 0.0f, static_cast<float>(m_RenderHeight));
    }

    if (m_Layer) {
      m_Layer->OnResize();
    }
  }

  Application::Viewport Application::ComputeLetterboxViewport() const {
    const float targetAspect = static_cast<float>(m_RenderWidth)       / static_cast<float>(m_RenderHeight);
    const float windowAspect = static_cast<float>(m_Window.GetWidth()) / static_cast<float>(m_Window.GetHeight());

    Viewport vp{ 0, 0, m_Window.GetWidth(), m_Window.GetHeight() };
    if (windowAspect > targetAspect) {
      // Window is wider than target  → black bars left and right
      vp.W = static_cast<int>(m_Window.GetHeight() * targetAspect);
      vp.X = (m_Window.GetWidth() - vp.W) / 2;
    } else {
      // Window is taller than target → black bars top and bottom
      vp.H = static_cast<int>(m_Window.GetWidth() / targetAspect);
      vp.Y = (m_Window.GetHeight() - vp.H) / 2;
    }
    return vp;
  }

// ── Perf HUD (debug builds only) ─────────────────────────────────────────────
#ifdef MARBLE_DEBUG

  void Application::TickPerfHUD(float dt) {
    // ── Record frame time into ring buffer ────────────────────────────────────
    m_FrameTimes[m_PerfHead] = dt * 1000.0f; // store in ms
    m_PerfHead = (m_PerfHead + 1) % k_PerfSamples;

    // ── Sample CPU usage and RAM every 0.5 s ─────────────────────────────────
    m_StatPollTimer += dt;
    if (m_StatPollTimer >= 0.5f) {
      m_StatPollTimer = 0.0f;

      FILETIME creation, exit, kernel, user;
      FILETIME sysTime;
      if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto ft64 = [](FILETIME ft) -> uint64_t {
          return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        GetSystemTimeAsFileTime(&sysTime);
        const uint64_t cpuNow  = ft64(kernel) + ft64(user);
        const uint64_t wallNow = ft64(sysTime);

        if (m_LastWallTime > 0 && wallNow > m_LastWallTime) {
          const uint64_t deltaCpu  = cpuNow  - m_LastCpuTotal;
          const uint64_t deltaWall = wallNow - m_LastWallTime;
          m_CpuUsagePct = static_cast<float>(deltaCpu)
                        / (static_cast<float>(deltaWall) * static_cast<float>(m_NumCores))
                        * 100.0f;
          m_CpuUsagePct = std::min(m_CpuUsagePct, 100.0f);
        }
        m_LastCpuTotal = cpuNow;
        m_LastWallTime = wallNow;
      }

      PROCESS_MEMORY_COUNTERS pmc{};
      if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_RamUsageMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
      }
    }

    // ── Build ImGui overlay window ────────────────────────────────────────────
    if (!m_PerfHUDVisible) return;

    float sum = 0.0f;
    for (float t : m_FrameTimes) { sum += t; }
    const float avgMs  = sum / static_cast<float>(k_PerfSamples);
    const float avgFps = avgMs > 0.0f ? 1000.0f / avgMs : 0.0f;

    constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDecoration        |
      ImGuiWindowFlags_NoInputs            |
      ImGuiWindowFlags_NoNav               |
      ImGuiWindowFlags_NoMove              |
      ImGuiWindowFlags_NoSavedSettings     |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos ({ 10.0f, 10.0f }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ 260.0f, 0.0f }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);

    if (ImGui::Begin("##PerfHUD", nullptr, kFlags)) {
      ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f },
        "FPS   %.1f  (%.2f ms)", avgFps, avgMs);

      const ImVec4 cpuColor = m_CpuUsagePct > 80.0f
        ? ImVec4{ 1.0f, 0.35f, 0.35f, 1.0f }
        : ImVec4{ 1.0f, 0.85f, 0.4f,  1.0f };
      ImGui::TextColored(cpuColor,
        "CPU   %.1f %%   |   RAM  %.1f MB", m_CpuUsagePct, m_RamUsageMB);

      ImGui::PlotLines("##ft", m_FrameTimes, k_PerfSamples, m_PerfHead,
                       nullptr, 0.0f, 33.4f, { 240.0f, 36.0f });

      ImGui::TextDisabled("F3  toggle");
    }
    ImGui::End();
  }

#endif // MARBLE_DEBUG

} // namespace Marble
