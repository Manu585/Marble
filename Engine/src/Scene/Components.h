#pragma once
#include "Component.h"
#include "Renderer/Texture.h"
#include "Renderer/Camera.h"
#include <glm/glm.hpp>

namespace Marble {

  struct TransformComponent : public Component {
    glm::vec2 Position = { 0.0f, 0.0f };
    float     Rotation = 0.0f;
    glm::vec2 Scale = { 1.0f, 1.0f };
  };

  struct SpriteComponent : public Component {
    Texture* Tex = nullptr;
    glm::vec2 Size = { 64.0f, 64.0f };
    glm::vec4 Tint = { 1.0f, 1.0f, 1.0f, 1.0f };
    int       Layer = 0;
  };

  struct CameraFollowComponent : public Component {
    // Camera to follow the entity. Must be set before the scene starts.
    OrthographicCamera* Camera = nullptr;
    glm::vec2           Offset = { 0.0f, 0.0f };

    void OnUpdate(float /*deltaTime*/) override {
      if (!Camera) return;

      // Requires a TransformComponent on the same entity.
      if (!GetEntity().HasComponent<TransformComponent>()) return;

      auto& transform = GetEntity().GetComponent<TransformComponent>();
      Camera->SetPosition({ transform.Position + Offset, 0.0f });
    }
  };

}