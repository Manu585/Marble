#pragma once
#include "Component.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <stdexcept>
#include <cassert>

namespace Marble {

  class Entity {
  public:
    explicit Entity(std::string name)
      : m_Name(std::move(name)) {}

    Entity(const Entity&)            = delete;
    Entity& operator=(const Entity&) = delete;

    // ── Component API ────────────────────────────────────────────
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
      assert(!HasComponent<T>() && "AddComponent: entity already has this component type");
      auto component = std::make_unique<T>(std::forward<Args>(args)...);
      component->m_Entity = this;
      T& ref = *component;
      m_Components[std::type_index(typeid(T))] = std::move(component);
      return ref;
    }

    template<typename T>
    T& GetComponent() {
      auto it = m_Components.find(std::type_index(typeid(T)));
      if (it == m_Components.end())
        throw std::runtime_error("Component not found on entity: " + m_Name);
      return static_cast<T&>(*it->second);
    }

    template<typename T>
    const T& GetComponent() const {
      auto it = m_Components.find(std::type_index(typeid(T)));
      if (it == m_Components.end())
        throw std::runtime_error("Component not found on entity: " + m_Name);
      return static_cast<const T&>(*it->second);
    }

    template<typename T>
    bool HasComponent() const {
      return m_Components.count(std::type_index(typeid(T))) > 0;
    }

    template<typename T>
    void RemoveComponent() {
      m_Components.erase(std::type_index(typeid(T)));
    }

    // ── Lifecycle ────────────────────────────────────────────────
    void OnStart() {
      for (auto& [type, component] : m_Components)
        component->OnStart();
    }

    void OnUpdate(float deltaTime) {
      for (auto& [type, component] : m_Components)
        component->OnUpdate(deltaTime);
    }

    const std::string& GetName() const { return m_Name; }

  private:
    std::string m_Name;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;
  };

} // namespace Marble
