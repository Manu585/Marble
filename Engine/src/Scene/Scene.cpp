#include "Scene.h"
#include <algorithm>
#include <unordered_set>

namespace Marble {

  Entity& Scene::CreateEntity(const std::string& name) {
    auto  entity = std::make_unique<Entity>(name);
    Entity& ref  = *entity;
    m_Entities.push_back(std::move(entity));
    return ref;
  }

  void Scene::DestroyEntity(Entity& entity) {
    // Enqueue for deferred removal. Immediate removal during OnUpdate would
    // invalidate the iteration loop and cause undefined behaviour.
    m_PendingDestroy.push_back(&entity);
  }

  void Scene::FlushDestroyQueue() {
    if (m_PendingDestroy.empty()) return;

    // Build a hash set for O(1) lookup per entity instead of O(m) linear scan.
    const std::unordered_set<Entity*> deadSet(m_PendingDestroy.begin(), m_PendingDestroy.end());
    m_Entities.erase(
      std::remove_if(m_Entities.begin(), m_Entities.end(),
        [&deadSet](const std::unique_ptr<Entity>& e) {
          return deadSet.count(e.get()) > 0;
        }),
      m_Entities.end()
    );
    m_PendingDestroy.clear();
  }

  void Scene::OnStart() {
    for (auto& entity : m_Entities)
      entity->OnStart();
  }

  void Scene::OnUpdate(float deltaTime) {
    for (auto& entity : m_Entities)
      entity->OnUpdate(deltaTime);

    FlushDestroyQueue();
  }

  void Scene::Render(Renderer2D& renderer) {
    if (!m_Camera) return;

    // Collect all sprite components and sort by layer so higher layers draw on top.
    // m_DrawList is a member — reused each frame to avoid per-frame heap allocation.
    m_DrawList.clear();

    for (auto& entity : m_Entities) {
      if (!entity->HasComponent<SpriteComponent>()) continue;

      auto& sprite = entity->GetComponent<SpriteComponent>();

      TransformComponent* transform = entity->HasComponent<TransformComponent>()
        ? &entity->GetComponent<TransformComponent>()
        : nullptr;

      m_DrawList.push_back({ &sprite, transform });
    }

    std::sort(m_DrawList.begin(), m_DrawList.end(),
      [](const DrawPair& a, const DrawPair& b) {
        return a.first->Layer < b.first->Layer;
      });

    renderer.BeginScene(*m_Camera);

    for (auto& [sprite, transform] : m_DrawList) {
      const glm::vec2 position = transform ? transform->Position : glm::vec2{ 0.0f, 0.0f };
      const float     rotation = transform ? transform->Rotation : 0.0f;
      const glm::vec2 size     = sprite->Size * (transform ? transform->Scale : glm::vec2{ 1.0f, 1.0f });

      if (sprite->Tex)
        renderer.DrawQuad(position, size, rotation, *sprite->Tex, sprite->Tint);
      else
        renderer.DrawQuad(position, size, rotation, sprite->Tint);
    }

    renderer.EndScene();
  }

}
