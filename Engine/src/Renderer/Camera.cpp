#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Marble {

  OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top) {
    m_Projection     = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    m_View           = glm::mat4(1.0f);
    m_ViewProjection = m_Projection * m_View;
  }

  void OrthographicCamera::SetProjection(float left, float right, float bottom, float top) {
    m_Projection     = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    m_ViewProjection = m_Projection * m_View;
  }

  void OrthographicCamera::SetPosition(const glm::vec3& position) {
    m_Position = position;
    RecalculateViewMatrix();
  }

  void OrthographicCamera::SetRotation(float degrees) {
    m_Rotation = degrees;
    RecalculateViewMatrix();
  }

  void OrthographicCamera::SetTransform(const glm::vec3& position, float degrees) {
    m_Position = position;
    m_Rotation = degrees;
    RecalculateViewMatrix();
  }

  void OrthographicCamera::RecalculateViewMatrix() {
    // Analytical inverse of (T * R): view = R^-1 * T^-1 = rotate(-angle) * translate(-pos).
    // This avoids a full 4x4 LU decomposition (glm::inverse) for a simple 2D camera.
    m_View = glm::rotate(glm::mat4(1.0f), -glm::radians(m_Rotation), glm::vec3(0, 0, 1))
           * glm::translate(glm::mat4(1.0f), -m_Position);
    m_ViewProjection = m_Projection * m_View;
  }

} // namespace Marble