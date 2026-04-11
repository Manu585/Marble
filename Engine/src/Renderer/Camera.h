#pragma once
#include <glm/glm.hpp>

namespace Marble {

  class OrthographicCamera {
  public:
    OrthographicCamera(float left, float right, float bottom, float top);

    void SetProjection(float left, float right, float bottom, float top);
    void SetPosition(const glm::vec3& position);
    // 2D convenience: equivalent to SetPosition({position.x, position.y, 0.0f})
    void SetPosition(const glm::vec2& position);
    void SetRotation(float degrees);
    // Sets both position and rotation in one call — avoids recalculating twice
    void SetTransform(const glm::vec3& position, float degrees = 0.0f);

    const glm::vec3& GetPosition() const {
      return m_Position;
    }

    float GetRotation() const {
      return m_Rotation;
    }

    const glm::mat4& GetViewMatrix() const {
      return m_View;
    }

    const glm::mat4& GetProjectionMatrix() const {
      return m_Projection;
    }

    const glm::mat4& GetViewProjection() const {
      return m_ViewProjection;
    }

  private:
    void RecalculateViewMatrix();

    glm::mat4 m_Projection;
    glm::mat4 m_View;
    glm::mat4 m_ViewProjection;

    glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
    float m_Rotation = 0.0f;
  };

} // namespace Marble