#pragma once

namespace Marble {

  class Entity;

  class Component {
  public:
    virtual ~Component() = default;

    virtual void OnStart()                 {}
    virtual void OnUpdate(float deltaTime) {}

    Entity&       GetEntity()       { return *m_Entity; }
    const Entity& GetEntity() const { return *m_Entity; }

  private:
    friend class Entity;
    Entity* m_Entity = nullptr;
  };

}
