#pragma once
#include "Entity.h"
#include "Components.h"
#include "Renderer/Camera.h"
#include "Renderer/Renderer2D.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <utility>

namespace Marble {

  class Scene {
  public:
    Scene()  = default;
    ~Scene() = default;

    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;

    Entity& CreateEntity(const std::string& name = "Entity");

    // Queues the entity for destruction. It is removed after the current
    // OnUpdate completes, so it is safe to call from a component's OnUpdate.
    void DestroyEntity(Entity& entity);

    // Non-owning pointer — the camera is owned by the game layer.
    void                SetCamera(OrthographicCamera* camera) { m_Camera = camera; }
    OrthographicCamera* GetCamera() const                     { return m_Camera;   }

    void OnStart();
    void OnUpdate(float deltaTime);
    void Render(Renderer2D& renderer);

  private:
    using DrawPair = std::pair<SpriteComponent*, TransformComponent*>;

    void FlushDestroyQueue();

    std::vector<std::unique_ptr<Entity>> m_Entities;
    std::vector<Entity*>                 m_PendingDestroy; // deferred destroy queue
    std::vector<DrawPair>                m_DrawList;       // reused each Render, avoids per-frame alloc
    OrthographicCamera*                  m_Camera = nullptr; // non-owning
  };

} // namespace Marble
