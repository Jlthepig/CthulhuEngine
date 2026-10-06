#pragma once

#include <glm.hpp>
namespace flecs
{
struct entity;
}
namespace JPH
{
class CharacterVirtual;
}
namespace Cthulhu::Physics
{
class PhysicsWorld;
}
namespace Cthulhu::Physics
{

class CharacterController
{
  public:
    static bool createRuntime(flecs::entity entity, PhysicsWorld &physicsWorld);

    static void destroyRuntime(flecs::entity entity);

    static void teleport(flecs::entity entity, const glm::vec3 &position);

    static void destroyCharacter(JPH::CharacterVirtual *character);
    static int getLiveCharacterCount();
};

} // namespace Cthulhu::Physics