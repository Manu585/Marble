#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

struct GLFWwindow; // forward declaration — keeps GLFW out of this header

namespace Marble {

  // Mirrors GLFW key codes — use these constants instead of raw integers.
  namespace Key {
    // ── Printable ────────────────────────────────────────────────
    inline constexpr int32_t Space     = 32;
    inline constexpr int32_t Apostrophe = 39;
    inline constexpr int32_t Comma     = 44;
    inline constexpr int32_t Minus     = 45;
    inline constexpr int32_t Period    = 46;
    inline constexpr int32_t Slash     = 47;

    // ── Digits ───────────────────────────────────────────────────
    inline constexpr int32_t Num0 = 48;
    inline constexpr int32_t Num1 = 49;
    inline constexpr int32_t Num2 = 50;
    inline constexpr int32_t Num3 = 51;
    inline constexpr int32_t Num4 = 52;
    inline constexpr int32_t Num5 = 53;
    inline constexpr int32_t Num6 = 54;
    inline constexpr int32_t Num7 = 55;
    inline constexpr int32_t Num8 = 56;
    inline constexpr int32_t Num9 = 57;

    // ── Letters ──────────────────────────────────────────────────
    inline constexpr int32_t A = 65;
    inline constexpr int32_t B = 66;
    inline constexpr int32_t C = 67;
    inline constexpr int32_t D = 68;
    inline constexpr int32_t E = 69;
    inline constexpr int32_t F = 70;
    inline constexpr int32_t G = 71;
    inline constexpr int32_t H = 72;
    inline constexpr int32_t I = 73;
    inline constexpr int32_t J = 74;
    inline constexpr int32_t K = 75;
    inline constexpr int32_t L = 76;
    inline constexpr int32_t M = 77;
    inline constexpr int32_t N = 78;
    inline constexpr int32_t O = 79;
    inline constexpr int32_t P = 80;
    inline constexpr int32_t Q = 81;
    inline constexpr int32_t R = 82;
    inline constexpr int32_t S = 83;
    inline constexpr int32_t T = 84;
    inline constexpr int32_t U = 85;
    inline constexpr int32_t V = 86;
    inline constexpr int32_t W = 87;
    inline constexpr int32_t X = 88;
    inline constexpr int32_t Y = 89;
    inline constexpr int32_t Z = 90;

    // ── Special / control ────────────────────────────────────────
    inline constexpr int32_t Escape    = 256;
    inline constexpr int32_t Enter     = 257;
    inline constexpr int32_t Tab       = 258;
    inline constexpr int32_t Backspace = 259;
    inline constexpr int32_t Insert    = 260;
    inline constexpr int32_t Delete    = 261;
    inline constexpr int32_t Right     = 262;
    inline constexpr int32_t Left      = 263;
    inline constexpr int32_t Down      = 264;
    inline constexpr int32_t Up        = 265;
    inline constexpr int32_t PageUp    = 266;
    inline constexpr int32_t PageDown  = 267;
    inline constexpr int32_t Home      = 268;
    inline constexpr int32_t End       = 269;

    // ── Function keys ────────────────────────────────────────────
    inline constexpr int32_t F1  = 290;
    inline constexpr int32_t F2  = 291;
    inline constexpr int32_t F3  = 292;
    inline constexpr int32_t F4  = 293;
    inline constexpr int32_t F5  = 294;
    inline constexpr int32_t F6  = 295;
    inline constexpr int32_t F7  = 296;
    inline constexpr int32_t F8  = 297;
    inline constexpr int32_t F9  = 298;
    inline constexpr int32_t F10 = 299;
    inline constexpr int32_t F11 = 300;
    inline constexpr int32_t F12 = 301;

    // ── Modifiers ────────────────────────────────────────────────
    inline constexpr int32_t LeftShift  = 340;
    inline constexpr int32_t LeftCtrl   = 341;
    inline constexpr int32_t LeftAlt    = 342;
    inline constexpr int32_t RightShift = 344;
    inline constexpr int32_t RightCtrl  = 345;
    inline constexpr int32_t RightAlt   = 346;
  }

  namespace Mouse {
    inline constexpr int32_t Left   = 0;
    inline constexpr int32_t Right  = 1;
    inline constexpr int32_t Middle = 2;
  }

  class Input {
  public:
    // IsKeyPressed      — true every frame the key is held
    // IsKeyJustPressed  — true only on the first frame the key is pressed (rising edge)
    // IsKeyJustReleased — true only on the first frame the key is released (falling edge)
    static bool IsKeyPressed      (int32_t key);
    static bool IsKeyJustPressed  (int32_t key);
    static bool IsKeyJustReleased (int32_t key);

    static bool IsMousePressed      (int32_t button);
    static bool IsMouseJustPressed  (int32_t button);
    static bool IsMouseJustReleased (int32_t button);

    // Mouse position is captured once per frame in BeginFrame — no per-call GLFW overhead.
    static glm::vec2 GetMousePosition();
    static float     GetMouseX();
    static float     GetMouseY();

    // ── Engine internals — do NOT call from game code ─────────────
    static void SetWindowHandle(GLFWwindow* window);
    static void BeginFrame(); // called by Application at the top of each frame

  private:
    static constexpr int32_t KEY_COUNT   = 512;
    static constexpr int32_t MOUSE_COUNT = 8;

    static GLFWwindow* s_Window;

    static std::array<bool, KEY_COUNT>   s_Keys;
    static std::array<bool, KEY_COUNT>   s_PrevKeys;
    static std::array<bool, MOUSE_COUNT> s_Mouse;
    static std::array<bool, MOUSE_COUNT> s_PrevMouse;
    static glm::vec2                     s_MousePos; // cached once per BeginFrame
  };

} // namespace Marble
