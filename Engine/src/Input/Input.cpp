#include "Input.h"
#include <GLFW/glfw3.h>

namespace Marble {

  GLFWwindow* Input::s_Window = nullptr;

  std::array<bool, Input::KEY_COUNT>   Input::s_Keys     = {};
  std::array<bool, Input::KEY_COUNT>   Input::s_PrevKeys  = {};
  std::array<bool, Input::MOUSE_COUNT> Input::s_Mouse     = {};
  std::array<bool, Input::MOUSE_COUNT> Input::s_PrevMouse = {};
  glm::vec2                            Input::s_MousePos  = {};

  void Input::SetWindowHandle(GLFWwindow* window) {
    s_Window = window;
  }

  void Input::BeginFrame() {
    s_PrevKeys  = s_Keys;
    s_PrevMouse = s_Mouse;

    for (int32_t i = 0; i < KEY_COUNT; i++)
      s_Keys[i] = glfwGetKey(s_Window, i) == GLFW_PRESS;

    for (int32_t i = 0; i < MOUSE_COUNT; i++)
      s_Mouse[i] = glfwGetMouseButton(s_Window, i) == GLFW_PRESS;

    // Cache mouse position once — avoids duplicate GLFW calls when
    // both GetMouseX() and GetMouseY() are called in the same frame.
    double x, y;
    glfwGetCursorPos(s_Window, &x, &y);
    s_MousePos = { static_cast<float>(x), static_cast<float>(y) };
  }

  bool Input::IsKeyPressed      (int32_t key) {
    if (key < 0 || key >= KEY_COUNT) return false;
    return s_Keys[key];
  }

  bool Input::IsKeyJustPressed  (int32_t key) {
    if (key < 0 || key >= KEY_COUNT) return false;
    return s_Keys[key] && !s_PrevKeys[key];
  }

  bool Input::IsKeyJustReleased (int32_t key) {
    if (key < 0 || key >= KEY_COUNT) return false;
    return !s_Keys[key] && s_PrevKeys[key];
  }

  bool Input::IsMousePressed      (int32_t button) {
    if (button < 0 || button >= MOUSE_COUNT) return false;
    return s_Mouse[button];
  }

  bool Input::IsMouseJustPressed  (int32_t button) {
    if (button < 0 || button >= MOUSE_COUNT) return false;
    return s_Mouse[button] && !s_PrevMouse[button];
  }

  bool Input::IsMouseJustReleased (int32_t button) {
    if (button < 0 || button >= MOUSE_COUNT) return false;
    return !s_Mouse[button] && s_PrevMouse[button];
  }

  glm::vec2 Input::GetMousePosition() { return s_MousePos; }
  float     Input::GetMouseX()        { return s_MousePos.x; }
  float     Input::GetMouseY()        { return s_MousePos.y; }

} // namespace Marble
